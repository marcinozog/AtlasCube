'use strict';

// SCREEN_SD — mirrors screen_sd_player.c.
//
// Structurally a sibling of the radio screen: the same wheels, VU, needles, clock,
// indicators, volume slider and hotspots, over its own sd_* fields. What is its
// own: the title/folder boxes, the status flags, the elapsed counter, the progress
// bar and the album cover.

const SD_INFO_BOX_W = () => S.meta.screen_w - 20;   // screen_sd_player.c

// The playback position lives in sd_player_position_ms() on the device and is not
// in the state broadcast, so neither the counter nor the bar can be truthful here.
// Both are drawn with real geometry at a representative position rather than left
// out — the point of the preview is where things sit, and an absent bar would
// misrepresent the layout more than a staged one does.
const SD_DEMO_PROGRESS = 0.38;

// ── Album cover — sd_cover_widget.c ─────────────────────────────────────────
// "cover.bin" alongside the tracks, so the artwork follows the folder, not the
// track. Reloaded only when the folder changes, for the same reason the station
// icon is: a state broadcast arrives on every volume tick.
let coverLoadedDir = null;

async function refreshSdCover() {
    const el = S.els.cover;
    if (!el) return;
    const dir = S.live.sd_active ? String(S.live.sd_dir || '') : '';
    if (dir === coverLoadedDir) return;
    coverLoadedDir = dir;

    if (!dir) {
        el.style.backgroundImage = '';
        el.style.display = 'none';
        return;
    }
    try {
        const rel = (dir.startsWith('/sdcard/') ? dir.slice('/sdcard'.length) : dir)
                        .replace(/\/$/, '') + '/cover.bin';
        const f = await fetch('/api/sd/file?path=' + encodeURIComponent(rel), { cache: 'no-store' });
        if (!f.ok) throw new Error('HTTP ' + f.status);
        const dec = window.LvBin.decodeToCanvas(await f.arrayBuffer());
        if (coverLoadedDir !== dir) return;      // folder changed mid-download
        el.style.backgroundImage = `url("${dec.canvas.toDataURL('image/png')}")`;
        el.style.backgroundSize  = '100% 100%';
        el.style.display = '';
    } catch {
        // A folder without artwork is the normal case, not an error.
        el.style.backgroundImage = '';
        el.style.display = 'none';
        coverLoadedDir = null;
    }
}

// ── Screen ──────────────────────────────────────────────────────────────────

async function renderSdScreen() {
    const p    = S.prof.sd;
    const th   = S.pal;
    const opa  = clamp(p.sd_label_bg_opa ?? 50, 0, 100);
    const frag = document.createDocumentFragment();

    renderSharedStubs(frag, p, 'sd');

    if (p.sd_show_cover) {
        const size = clamp(p.sd_cover_size | 0, 16, 240);
        S.els.cover = box(p.sd_cover_x | 0, p.sd_cover_y | 0, size, size, { display: 'none' });
        coverLoadedDir = null;
        frag.appendChild(S.els.cover);
    }

    // Title and folder are fixed-width boxes (like bt_title): text centred on the
    // box centre and capped at the box width, so neither spills off its box.
    const titleW = Math.max(p.sd_title_w | 0, 8);
    S.els.sdTitle = makeLabel({
        x: (p.sd_title_x | 0) + titleW / 2, y: p.sd_title_y | 0,
        fontId: p.sd_title_font, text: '', boxW: titleW, plate: opa,
        color: col(p.sd_title_color, th.text_primary),
    });
    frag.appendChild(S.els.sdTitle);

    if (p.sd_show_folder) {
        const folderW = Math.max(p.sd_folder_w | 0, 8);
        S.els.sdFolder = makeLabel({
            x: (p.sd_folder_x | 0) + folderW / 2, y: p.sd_folder_y | 0,
            fontId: p.sd_folder_font, text: '', boxW: folderW, plate: opa,
            color: col(p.sd_folder_color, th.accent),
        });
        frag.appendChild(S.els.sdFolder);
    }

    // Volume, status flags and the counter are independent centre-anchored labels
    // sharing sd_info_font, each boxed to SD_INFO_BOX_W.
    const infoColor = col(p.sd_info_color, th.text_muted);
    const info = (key, xf, yf) => {
        S.els[key] = makeLabel({
            x: p[xf] | 0, y: p[yf] | 0, fontId: p.sd_info_font, text: '',
            boxW: SD_INFO_BOX_W(), plate: opa, color: infoColor,
        });
        frag.appendChild(S.els[key]);
    };
    if (p.sd_volume_show) info('sdVolume', 'sd_volume_x', 'sd_volume_y');
    if (p.sd_status_show) info('sdStatus', 'sd_status_x', 'sd_status_y');
    if (p.sd_show_time)   info('sdTime',   'sd_time_x',   'sd_time_y');

    // Read-only progress bar: muted track at 40 % opacity, accent indicator, both
    // with a fully rounded end (radius = height / 2).
    if (p.sd_show_bar && (p.sd_bar_w | 0) > 0) {
        const bw = p.sd_bar_w | 0, bh = p.sd_bar_h | 0;
        const r  = Math.floor(bh / 2) + 'px';
        const bar = box(p.sd_bar_x | 0, p.sd_bar_y | 0, bw, bh, {
            background: rgba(th.text_muted, 40 / 255), borderRadius: r, overflow: 'hidden',
        });
        bar.appendChild(box(0, 0, Math.round(bw * SD_DEMO_PROGRESS), bh, {
            background: th.accent, borderRadius: r,
        }));
        frag.appendChild(bar);
    }

    renderClockWidget(frag, p, 'sd');
    renderHotspots(frag, p, 'sd');

    screenEl.replaceChildren(frag);
    await renderVolSlider(screenEl, p, 'sd');
    refreshLive();
}

// ── Live ────────────────────────────────────────────────────────────────────

function basenameOf(path) {
    if (!path) return '';
    const i = String(path).lastIndexOf('/');
    return i >= 0 ? String(path).slice(i + 1) : String(path);
}

function fmtMmss(ms) {
    const s = Math.floor(ms / 1000);
    return `${Math.floor(s / 60)}:${String(s % 60).padStart(2, '0')}`;
}

// repeat_str()
function repeatText(r) {
    return r === 1 ? 'REPEAT ALL' : (r === 2 ? 'REPEAT ONE' : '');
}

function refreshSdLive() {
    const L = S.live;

    // Only show a track while SD is the active source — app_state.title is shared
    // and otherwise holds the radio's ICY title. Blank (hidden) when idle, so no
    // empty plate shows; "Nothing playing" in the folder line covers it.
    setLabelText(S.els.sdTitle,
        L.sd_active ? (L.title || L.sd_track || '—') : '');

    if (S.els.sdFolder) {
        setLabelText(S.els.sdFolder,
            L.sd_active
                ? ((L.sd_count | 0) > 0
                    ? `${basenameOf(L.sd_dir)}   ${(L.sd_index | 0) + 1}/${L.sd_count | 0}`
                    : basenameOf(L.sd_dir))
                : 'Nothing playing');
    }

    if (S.els.sdVolume) setLabelText(S.els.sdVolume, `VOL: ${L.volume | 0}%`);

    if (S.els.sdStatus) {
        // Flags only — hidden (empty) when nothing is active, so no plate shows.
        const flags = [];
        if (L.sd_paused)  flags.push('PAUSED');
        if (L.sd_shuffle) flags.push('SHUFFLE');
        const r = repeatText(L.sd_repeat | 0);
        if (r) flags.push(r);
        setLabelText(S.els.sdStatus, flags.join('   '));
    }

    // Staged, like the bar: without a position there is nothing true to show, and
    // an empty line would move every label judged against it.
    if (S.els.sdTime) {
        setLabelText(S.els.sdTime,
            L.sd_active ? `${fmtMmss(74000)} / ${fmtMmss(195000)}` : '');
    }

    refreshSdCover();
}
