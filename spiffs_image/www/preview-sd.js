'use strict';

// SCREEN_SD — mirrors screen_sd_player.c.
//
// Structurally a sibling of the radio screen: the same wheels, VU, needles, clock,
// indicators, volume slider and hotspots, over its own sd_* fields. What is its
// own: the title/folder boxes, the status flags, the elapsed counter, the progress
// bar and the album cover.

const SD_INFO_BOX_W = () => S.meta.screen_w - 20;   // screen_sd_player.c

// Fallback for firmware that predates sd_position_ms / sd_duration_ms: the layout
// still has to read, and an absent bar would misrepresent it more than a staged
// one does. Marked as staged on the page whenever it is in use.
const SD_DEMO_PROGRESS = 0.38;

// Live position, extrapolated the way the protocol doc prescribes: the field is a
// wall-clock delta, so adding the time elapsed since the frame arrived reproduces
// what sd_player_position_ms() would return right now. Frozen while paused,
// exactly as the device freezes it at s_pause_start_us.
function sdProgress() {
    const L = S.live;
    if (typeof L.sd_position_ms !== 'number') return null;   // old firmware
    if (!L.sd_active) return { pos: 0, dur: 0 };

    let pos = L.sd_position_ms;
    if (!L.sd_paused) pos += performance.now() - S.sdPosAt;

    const dur = L.sd_duration_ms | 0;
    // progress_update() clamps a position that has run past a known duration.
    if (dur && pos > dur) pos = dur;
    return { pos, dur };
}

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

    // Read-only progress bar, both parts with a fully rounded end (radius =
    // height / 2). Colours follow the profile overrides; the theme default track
    // keeps its 40 % wash, a track colour picked on purpose is painted solid.
    if (p.sd_show_bar && (p.sd_bar_w | 0) > 0) {
        const bw = p.sd_bar_w | 0, bh = p.sd_bar_h | 0;
        const r  = Math.floor(bh / 2) + 'px';
        const track = p.sd_bar_bg_color ? col(p.sd_bar_bg_color, th.text_muted)
                                        : rgba(th.text_muted, 40 / 255);
        const bar = box(p.sd_bar_x | 0, p.sd_bar_y | 0, bw, bh, {
            background: track, borderRadius: r, overflow: 'hidden',
        });
        S.els.sdBarFill = box(0, 0, 0, bh, {
            background: col(p.sd_bar_color, th.accent), borderRadius: r,
        });
        bar.appendChild(S.els.sdBarFill);
        S.els.sdBar = bar;
        S.els.sdBarW = bw;
        frag.appendChild(bar);
    }

    renderClockWidget(frag, p, 'sd');
    renderHotspots(frag, p, 'sd');

    screenEl.replaceChildren(frag);
    await renderVolSlider(screenEl, p, 'sd');

    // The device drives both the counter and the bar off a 1 Hz lv_timer created
    // when either exists; the same beat here keeps the extrapolation moving
    // between state broadcasts.
    clearInterval(sdTick);
    sdTick = (S.els.sdTime || S.els.sdBar) ? setInterval(updateSdProgress, 1000) : null;

    refreshLive();
}

let sdTick = null;

function updateSdProgress() {
    const L = S.live;
    const live = sdProgress();

    if (S.els.sdTime) {
        if (!L.sd_active) {
            setLabelText(S.els.sdTime, '');
        } else if (live) {
            // "elapsed / total", or elapsed alone until the length is known.
            setLabelText(S.els.sdTime, live.dur
                ? `${fmtMmss(live.pos)} / ${fmtMmss(live.dur)}`
                : fmtMmss(live.pos));
        } else {
            setLabelText(S.els.sdTime, `${fmtMmss(74000)} / ${fmtMmss(195000)}`);
        }
    }

    if (S.els.sdBar) {
        // The bar needs a known length; formats without one keep it hidden and
        // show the elapsed counter alone.
        const show = live ? (L.sd_active && live.dur > 0) : true;
        S.els.sdBar.style.display = show ? '' : 'none';
        if (show) {
            const frac = live ? live.pos / live.dur : SD_DEMO_PROGRESS;
            S.els.sdBarFill.style.width = Math.round(S.els.sdBarW * clamp(frac, 0, 1)) + 'px';
        }
    }
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

    updateSdProgress();   // snap the counter and bar on track/source change
    refreshSdCover();
}
