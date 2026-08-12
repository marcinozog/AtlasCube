'use strict';

// The list screens — SCREEN_PLAYLIST and SCREEN_SD_BROWSER.
//
// One renderer for both on purpose: they are the same widget. `browser_*` mirrors
// `playlist_*` field for field, both go through ui_list_widget.c and both size
// their box with list_box_of(). Only the header text and where the rows come from
// differ, so those are the only things parameterised here.

const UI_LIST_BOX_PAD = 2;   // ui_profile.h

// LV_SYMBOL_* are FontAwesome glyphs in the private use area. They are not in the
// Montserrat subset this page ships, so the browser's row icons are stand-ins
// drawn from the system font — the shapes differ from the panel's.
const ROW_ICON = { up: '↑', folder: '▸', track: '♪' };

// list_box_of(): a box with both dimensions set is taken as given; otherwise the
// list fills the panel under the header.
function listBoxOf(p, pre) {
    const bw = p[`${pre}_list_w`] | 0, bh = p[`${pre}_list_h`] | 0;
    if (bw > 0 && bh > 0) {
        return { x: p[`${pre}_list_x`] | 0, y: p[`${pre}_list_y`] | 0, w: bw, h: bh };
    }
    const top = p[`${pre}_header_hide`] ? 0 : (p[`${pre}_header_h`] | 0);
    return { x: 0, y: top, w: S.meta.screen_w, h: S.meta.screen_h - top };
}

// build_order(): favourites first, original order preserved within each group.
// The mapping matters — app_state.curr_index is a REAL index, while the cursor
// and the row numbering live in display space.
function playlistOrder() {
    const fav = [], rest = [];
    S.playlist.forEach((e, i) => (e.favorite ? fav : rest).push(i));
    return fav.concat(rest);
}

// bind_row(): "%c%2d. %s" — '*' marks a favourite, the number is display position.
function playlistRowText(order, dispIdx) {
    const e = S.playlist[order[dispIdx]];
    const star = (e && e.favorite) ? '*' : ' ';
    return `${star}${String(dispIdx + 1).padStart(2, ' ')}. ${e ? e.name : ''}`;
}

// Rows for the active mode: text, whether the row takes the accent colour, what a
// click does, and which row the cursor starts on.
function listRows() {
    if (S.mode === 'radio') {
        const order = playlistOrder();
        const playing = order.indexOf(S.live.curr_index | 0);
        return {
            selected: playing >= 0 ? playing : 0,
            rows: order.map((realIdx, i) => ({
                text: playlistRowText(order, i),
                accent: i === playing,
                click: () => {
                    // play_display_index(): the index sent is the REAL one, and the
                    // device skips the call when that station is already playing.
                    if (realIdx === (S.live.curr_index | 0) && S.live.radio === 'playing') return;
                    if (!wsSend({ cmd: 'play_index', index: realIdx })) {
                        setWsBadge('down', 'not sent — no connection');
                    }
                },
            })),
        };
    }

    // SD browser: the entries are the device's own scan of one folder, in the
    // order populate() builds them — up, then folders, then tracks. Navigation
    // entries stand out from playable files (bind_row sets their colour).
    const d = S.sdList;
    if (!d) return { selected: 0, rows: [] };
    const rows = [];
    if (d.parent) {
        rows.push({
            text: `${ROW_ICON.up}  ..`, accent: true,
            click: () => requestSdList(d.parent),
        });
    }
    for (const f of (d.folders || [])) {
        rows.push({
            text: `${ROW_ICON.folder} ${f}`, accent: true,
            click: () => requestSdList((d.dir.endsWith('/') ? d.dir : d.dir + '/') + f),
        });
    }
    for (const t of (d.tracks || [])) {
        rows.push({
            text: `${ROW_ICON.track} ${t}`, accent: false,
            click: () => {
                const path = (d.dir.endsWith('/') ? d.dir : d.dir + '/') + t;
                if (!wsSend({ cmd: 'sd_play_path', path })) {
                    setWsBadge('down', 'not sent — no connection');
                }
            },
        });
    }
    // ui_list_set_count() resets the cursor to 0 after every rescan.
    return { selected: 0, rows };
}

function renderListScreen() {
    const cfg = modeCfg();
    const pre = cfg.list;
    const p   = S.prof[pre];
    if (!p) return;
    const th   = S.pal;
    const frag = document.createDocumentFragment();

    // ----- Header strip -----
    if (!p[`${pre}_header_hide`]) {
        const hh = p[`${pre}_header_h`] | 0;
        const header = box(0, 0, S.meta.screen_w, hh, {
            background: th.bg_secondary, overflow: 'hidden',
        });

        // LV_ALIGN_LEFT_MID / RIGHT_MID plus the configured offset. The label is
        // centred on the strip's middle, so its top comes from the font box.
        const hf = lvMetrics(p[`${pre}_header_font`]);
        header.appendChild(alignedText({
            text: pre === 'playlist' ? 'Playlist' : 'SD',
            fontId: p[`${pre}_header_font`], color: th.accent,
            left: p[`${pre}_label_x`] | 0,
            top: Math.round((hh - hf.lh) / 2) + (p[`${pre}_label_y`] | 0),
        }));

        if (!p[`${pre}_hint_hide`]) {
            const rf = lvMetrics(p[`${pre}_row_font`]);
            const hint = alignedText({
                text: pre === 'playlist' ? 'press - play   swipe<>/long - exit'
                                         : 'press - open   swipe<>/long - back',
                fontId: p[`${pre}_row_font`], color: th.text_muted,
                top: Math.round((hh - rf.lh) / 2) + (p[`${pre}_hint_y`] | 0),
            });
            // RIGHT_MID: the offset is measured from the strip's right edge.
            hint.style.right = (-(p[`${pre}_hint_x`] | 0)) + 'px';
            header.appendChild(hint);
        }
        frag.appendChild(header);
    }

    // ----- List -----
    const bx    = listBoxOf(p, pre);
    const pitch = Math.max((p[`${pre}_item_h`] | 0) + (p[`${pre}_item_pad`] | 0), 1);
    const itemH = p[`${pre}_item_h`] | 0;
    const viewH = Math.max(bx.h - 2 * UI_LIST_BOX_PAD, 1);
    const rowW  = Math.max(bx.w - 2 * UI_LIST_BOX_PAD, 8);

    const { rows, selected } = listRows();
    const count = rows.length;

    // ui_list_select(): centre the selection, clamped to the scroll range.
    const maxScroll = Math.max(count * pitch - viewH, 0);
    const scrollY = clamp(selected * pitch + Math.floor(pitch / 2) - Math.floor(viewH / 2),
                          0, maxScroll);

    // The viewport is transparent — the wallpaper shows between and around rows —
    // and scrolls vertically, like lv_obj_set_scroll_dir(LV_DIR_VER). Padding goes
    // on the box itself (pad_all UI_LIST_BOX_PAD) with border-box sizing, so the
    // content area comes out as row_w x view_h exactly as the widget computes them.
    const view = box(bx.x, bx.y, bx.w, bx.h, {
        overflowY: 'auto', overflowX: 'hidden',
        boxSizing: 'border-box', padding: UI_LIST_BOX_PAD + 'px',
        touchAction: 'pan-y',
    });
    view.className = 'pl-view';

    const content = document.createElement('div');
    content.style.position = 'relative';
    content.style.width  = rowW + 'px';
    content.style.height = (count * pitch) + 'px';
    view.appendChild(content);

    // Drag-to-scroll for a mouse, so the list behaves on a laptop the way it does
    // under a finger. Touch is left to the browser's own panning — the pointer
    // events would otherwise scroll it twice.
    //
    // A drag that crosses the threshold cancels the click the browser sends on
    // release, mirroring row_click_cb()'s ui_swipe_fired() guard: on the panel too,
    // a swipe that started on a row must not be taken as picking that row.
    let dragFrom = null;
    let dragged  = false;

    view.addEventListener('pointerdown', (e) => {
        if (e.pointerType === 'touch') return;
        dragFrom = e.clientY;
        dragged  = false;
        view.setPointerCapture(e.pointerId);
    });
    view.addEventListener('pointermove', (e) => {
        if (dragFrom === null) return;
        const dy = e.clientY - dragFrom;
        if (Math.abs(dy) > 6) dragged = true;
        // Page pixels are device pixels times the zoom, and scrollTop is in the
        // scaled element's own coordinates.
        view.scrollTop -= dy / (S.scale || 1);
        dragFrom = e.clientY;
    });
    const endDrag = (e) => {
        if (dragFrom === null) return;
        dragFrom = null;
        if (view.hasPointerCapture(e.pointerId)) view.releasePointerCapture(e.pointerId);
    };
    view.addEventListener('pointerup', endDrag);
    view.addEventListener('pointercancel', endDrag);

    const rf     = lvMetrics(p[`${pre}_row_font`]);
    const padTop = Math.max(Math.floor((itemH - rf.lh) / 2), 0);
    const rowOpa = clamp(p[`${pre}_label_bg_opa`] ?? 100, 0, 100) / 100;

    // Every entry gets a node. The device recycles a pool of one screenful because
    // it has a software renderer to feed; a browser scrolling a few hundred divs
    // natively has no such problem, and virtualising here would add a second scroll
    // implementation to keep honest.
    for (let i = 0; i < count; i++) {
        const isCursor = i === selected;
        const bg = isCursor ? col(p[`${pre}_cursor_bg_color`], th.accent)
                            : col(p[`${pre}_row_bg_color`], th.bg_secondary);
        // style_row(): the cursor's own colours win; otherwise the row keeps the
        // colour bind_row() gave it.
        let fg = isCursor ? col(p[`${pre}_cursor_text_color`], '#ffffff')
                          : col(p[`${pre}_row_text_color`], th.text_primary);
        if (!isCursor && rows[i].accent) fg = col(p[`${pre}_row_accent_color`], th.accent);

        const row = box(0, i * pitch, rowW, itemH, { overflow: 'hidden' });
        row.style.background = rgba(bg, rowOpa);
        row.appendChild(alignedText({
            text: rows[i].text, fontId: p[`${pre}_row_font`], color: fg,
            left: p[`${pre}_row_pad_left`] | 0, top: padTop,
        }));
        row.className = 'pl-row';
        row.addEventListener('click', () => {
            if (dragged) return;   // that press was a scroll, not a pick
            rows[i].click();
        });
        content.appendChild(row);
    }

    frag.appendChild(view);
    listScreen.replaceChildren(frag);
    // Applied after the nodes are in the document — a detached element has nothing
    // to scroll.
    view.scrollTop = scrollY;
    listRenderedKey = listStateKey();
}

// The playlist's cursor and accent row key off curr_index; the browser's rows come
// from the last sd_list broadcast. Either way the list only needs rebuilding when
// one of those changes — not on every volume tick.
let listRenderedKey = null;

function listStateKey() {
    return S.mode === 'radio' ? 'r' + (S.live.curr_index | 0)
                              : 's' + (S.sdList ? S.sdList.dir : '');
}

function refreshListLive() {
    if (!S.prof[modeCfg().list]) return;
    if (listStateKey() !== listRenderedKey) renderListScreen();
}
