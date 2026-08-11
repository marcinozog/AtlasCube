'use strict';

// Browser rendering of SCREEN_RADIO as the panel actually shows it.
//
// Every number comes from the device, none of it is duplicated here:
//   /api/ui/profile/meta   — panel size + real LVGL line_height/base_line per font
//   /api/ui/profile/radio  — the geometry screen_radio.c builds from
//   /api/theme             — active palette (a profile colour of 0 inherits it)
//   /api/settings          — background tier (wallpaper / gradient / solid) + dim
//   ws://<host>/ws         — live station, title, volume, stream info
//
// What IS restated here are the firmware's drawing rules — where a centre-anchored
// label lands, how the scrim plate pads it, how the background tiers resolve. Each
// of those carries the name of the C function it mirrors, so the two can be checked
// against each other when a layout rule changes.
//
// Read-only by design: this page never POSTs and never sends a WS command.
// Deliberately separate from layout.js — that one draws a schematic for editing,
// this one draws the screen.

// ─────────────────────────────────────────────────────────────────────────────
// State
// ─────────────────────────────────────────────────────────────────────────────

const S = {
    meta:     null,   // /api/ui/profile/meta
    p:        null,   // /api/ui/profile/radio
    settings: null,   // /api/settings
    pal:      null,   // active palette from /api/theme
    playlist: [],     // /api/playlist — carries the per-station icon paths
    live: {           // last WS state, seeded with what the device shows at rest
        radio: 'stopped', station_name: '', title: '', volume: 0,
        sr: 0, ch: 2, br: 0, sd_active: false, curr_index: -1,
    },
    els: {},          // live-updating label nodes, keyed by role
    ws: null,
    wsRetry: 250,
};

const screenEl = document.getElementById('screen');
const stageEl  = document.getElementById('stage');

// ─────────────────────────────────────────────────────────────────────────────
// Colours
// ─────────────────────────────────────────────────────────────────────────────

// A ui_profile colour field is a packed RGB integer where 0 means "inherit the
// theme", exactly as every `p->radio_*_color ? … : th->…` in screen_radio.c reads it.
function col(value, fallbackHex) {
    const v = value | 0;
    if (!v) return fallbackHex;
    return '#' + (v & 0xFFFFFF).toString(16).padStart(6, '0');
}

function hexToRgb(hex) {
    const v = parseInt(String(hex).replace('#', ''), 16) || 0;
    return [(v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF];
}

function rgba(hex, alpha) {
    const [r, g, b] = hexToRgb(hex);
    return `rgba(${r},${g},${b},${alpha})`;
}

function clamp(v, lo, hi) { return v < lo ? lo : (v > hi ? hi : v); }

// ─────────────────────────────────────────────────────────────────────────────
// Fonts
// ─────────────────────────────────────────────────────────────────────────────

// Nominal pixel size out of the font id ("montserrat_18_eu" → 18). It is the
// em size lv_font_conv was given, which is also what CSS font-size means, so the
// glyphs come out at the same scale. The suffix is matched loosely on purpose:
// older firmware ships the same fonts as "_pl".
function fontPx(id) {
    const m = String(id || '').match(/_(\d+)(_[a-z]+)?$/);
    return m ? parseInt(m[1], 10) : 14;
}

// Box height and baseline the device uses. api_ui_profile_meta_get_handler sends
// the real lv_font_t line_height/base_line; the fallbacks are the ratios they
// average out to, for firmware predating the field (same fallbacks as layout.js).
function lvMetrics(id) {
    const px = fontPx(id);
    const m  = (S.meta.font_metrics || {})[id];
    const lh = m ? m.h : Math.round(px * 1.09);
    // LVGL puts the baseline base_line above the box bottom.
    const baseFromTop = m ? m.h - m.b : Math.round(lh * 0.82);
    return { px, lh, baseFromTop };
}

// Where the browser would put the baseline inside a line box of `lh` px: CSS
// splits the leftover leading evenly above and below the font's own ascent+descent.
const measureCtx = document.createElement('canvas').getContext('2d');

function browserBaseline(px, lh) {
    measureCtx.font = `500 ${px}px AtlasMontserrat`;
    const tm = measureCtx.measureText('Hxg');
    const a = tm.fontBoundingBoxAscent, d = tm.fontBoundingBoxDescent;
    if (!(a > 0)) return lh * 0.82;          // metrics unavailable — rough guess
    return (lh - (a + d)) / 2 + a;
}

// Per-font nudge that lands the browser's baseline exactly on the device's.
// Computed once the webfont has loaded, so the correction is measured rather
// than assumed — without it the text sits a pixel or two off and every judgement
// about the layout would be made against a lie.
const baselineFix = new Map();

function baselineOffset(id) {
    if (!baselineFix.has(id)) {
        const { px, lh, baseFromTop } = lvMetrics(id);
        baselineFix.set(id, baseFromTop - browserBaseline(px, lh));
    }
    return baselineFix.get(id);
}

// ─────────────────────────────────────────────────────────────────────────────
// Labels
// ─────────────────────────────────────────────────────────────────────────────

// One label as ui_anchored_label() + ui_label_scrim() produce it.
//
//   align 'center' → x is the horizontal middle of the object (ui_label.c
//                    on_size_changed: x -= w / 2, width including padding, which
//                    is what translateX(-50%) does over the border box)
//   align 'left'   → x is the left edge
//   boxW           → ui_label_set_text_boxed(): the label hugs its text but never
//                    grows past the box; text centred inside it
//   plate          → radio_label_bg_opa. ui_label_scrim() returns early at 0, so
//                    at 0 there is no padding either and the geometry is bare text.
function makeLabel({ x, y, fontId, text, color, align = 'center', boxW = 0, plate = 0 }) {
    const { px, lh } = lvMetrics(fontId);
    const el = document.createElement('div');

    el.style.position   = 'absolute';
    el.style.boxSizing  = 'content-box';
    el.style.whiteSpace = 'nowrap';
    el.style.overflow   = 'hidden';
    el.style.fontSize   = px + 'px';
    el.style.height     = lh + 'px';
    el.style.lineHeight = lh + 'px';
    el.style.color      = color;
    el.style.left       = x + 'px';
    el.style.top        = y + 'px';
    el.style.textAlign  = 'center';

    if (plate > 0) {
        // ui_label_scrim(): theme bg_primary at opa_pct, radius 8, pad 6 / 1.
        el.style.background   = rgba(S.pal.bg_primary, clamp(plate, 0, 100) / 100);
        el.style.borderRadius = '8px';
        el.style.padding      = '1px 6px';
    }
    if (boxW > 0) el.style.maxWidth = boxW + 'px';
    if (align === 'center') el.style.transform = 'translateX(-50%)';

    // The text rides in its own span so the baseline correction shifts the glyphs
    // without moving the plate.
    const span = document.createElement('span');
    span.style.position = 'relative';
    span.style.top      = baselineOffset(fontId).toFixed(2) + 'px';
    span.textContent    = text;
    el.appendChild(span);
    el.dataset.role = 'label';
    el._span = span;
    return el;
}

// ui_label_set_text(): empty text hides the whole label, so no empty plate shows.
function setLabelText(el, text) {
    if (!el) return;
    el._span.textContent = text;
    el.style.display = text ? '' : 'none';
}

// A widget this preview does not draw for real yet.
function stub(x, y, w, h, label) {
    const el = document.createElement('div');
    el.className = 'stub';
    el.style.left   = x + 'px';
    el.style.top    = y + 'px';
    el.style.width  = Math.max(w | 0, 8) + 'px';
    el.style.height = Math.max(h | 0, 8) + 'px';
    const tag = document.createElement('span');
    tag.textContent = label;
    el.appendChild(tag);
    return el;
}

function box(x, y, w, h, css) {
    const el = document.createElement('div');
    el.style.position = 'absolute';
    el.style.left   = x + 'px';
    el.style.top    = y + 'px';
    el.style.width  = w + 'px';
    el.style.height = h + 'px';
    Object.assign(el.style, css || {});
    return el;
}

// ─────────────────────────────────────────────────────────────────────────────
// Background — mirrors ui_background_apply() for SCREEN_RADIO
// ─────────────────────────────────────────────────────────────────────────────

// net_slot_of(): "net0".."net9" name a slot, bare "net" is the pre-slots spelling
// for slot 0; a path or "none" is not an internet override.
function netSlotOf(ovr) {
    if (!ovr || ovr.slice(0, 3) !== 'net') return -1;
    if (ovr.length === 3) return 0;
    if (ovr.length !== 4) return -1;
    const n = ovr.charCodeAt(3) - 48;
    return (n >= 0 && n <= 9) ? n : -1;
}

// Baked-in dim: the firmware multiplies the pixels by (100 - dim)% at load time,
// which is the same result as compositing black over them at dim% opacity.
function dimLayer(dimPct) {
    return dimPct > 0 ? `linear-gradient(rgba(0,0,0,${dimPct / 100}), rgba(0,0,0,${dimPct / 100}))` : '';
}

async function applyBackground() {
    const display = S.settings.display || {};
    const dim     = clamp(display.wallpaper_dim || 0, 0, 100);
    const ovr     = String(S.p.radio_wallpaper || '');
    const slot    = netSlotOf(ovr);
    const isPath  = ovr && ovr !== 'none' && slot < 0;
    const badge   = document.getElementById('dim_badge');

    const paint = (image, what) => {
        const layers = [dimLayer(dim), image].filter(Boolean).join(', ');
        screenEl.style.background     = layers || S.pal.bg_primary;
        screenEl.style.backgroundSize = 'cover';
        badge.textContent = dim > 0 ? `${what} · dim ${dim}%` : what;
    };

    // 1. Internet wallpaper — only when this screen is not pinned to an SD file.
    //    A screen pinned to a slot takes THAT slot; otherwise slot 0, the same
    //    fallback the firmware makes.
    if (!isPath) {
        try {
            const st   = await fetch('/api/wallpaper/status', { cache: 'no-store' });
            const info = st.ok ? await st.json() : null;
            if (info && info.active) {
                const img = await fetch('/api/wallpaper/image?slot=' + (slot >= 0 ? slot : 0),
                                        { cache: 'no-store' });
                if (img.ok) {
                    const dec = window.LvBin.decodeToCanvas(await img.arrayBuffer());
                    paint(`url("${dec.canvas.toDataURL('image/png')}")`,
                          'internet wallpaper' + (slot >= 0 ? ` (slot ${slot})` : ''));
                    return;
                }
            }
        } catch { /* no fetched wallpaper — fall through to the tiers below */ }
    }

    // 2. An explicit per-screen SD .bin. Note there is no global-wallpaper tier
    //    for radio: screen_wp_override() returns radio_wallpaper (never NULL), so
    //    the firmware's `!ovr` fallback to display.wallpaper_path cannot apply here.
    if (isPath) {
        try {
            const rel = ovr.startsWith('/sdcard/') ? ovr.slice('/sdcard'.length) : ovr;
            const f = await fetch('/api/sd/file?path=' + encodeURIComponent(rel), { cache: 'no-store' });
            if (f.ok) {
                const dec = window.LvBin.decodeToCanvas(await f.arrayBuffer());
                paint(`url("${dec.canvas.toDataURL('image/png')}")`, 'SD wallpaper');
                return;
            }
        } catch { /* unreadable file — the device falls back to the gradient too */ }
    }

    // 3. Solid, when the gradient is switched off.
    if (!display.bg_gradient) {
        screenEl.style.background = S.pal.bg_primary;
        badge.textContent = 'solid background';
        return;
    }

    // 4. Vertical palette gradient (the device dithers it into RGB565; the browser
    //    renders it in full colour, so banding differs — the colours do not).
    screenEl.style.background =
        `linear-gradient(${S.pal.bg_grad_top}, ${S.pal.bg_grad_bottom})`;
    badge.textContent = 'theme gradient';
}

// ─────────────────────────────────────────────────────────────────────────────
// Station icon — mirrors station_icon_widget.c
// ─────────────────────────────────────────────────────────────────────────────

// The artwork is not part of the profile: it hangs off the playlist entry of the
// station currently playing (playlist_get(curr_index)->icon_path), as a path on
// the SD card holding an LVGL .bin.
function currentIconPath() {
    const e = S.playlist[S.live.curr_index | 0];
    const rel = e && typeof e.icon === 'string' ? e.icon : '';
    // Same rejection the widget applies before it even touches the card.
    if (!rel || rel[0] !== '/' || rel.includes('..')) return '';
    return rel;
}

// Reload only on a real change of path. The device skips an unchanged path
// (strcmp against s_path) and so must this: a state broadcast arrives on every
// volume tick, and refetching the .bin each time would pound the SD card.
let iconLoadedPath = null;

async function refreshStationIcon() {
    const el = S.els.stationIcon;
    if (!el) return;
    const rel = currentIconPath();
    if (rel === iconLoadedPath) return;
    iconLoadedPath = rel;

    if (!rel) {                       // clear_icon(): hide, don't leave stale art
        el.style.backgroundImage = '';
        el.style.display = 'none';
        return;
    }
    try {
        const f = await fetch('/api/sd/file?path=' + encodeURIComponent(rel), { cache: 'no-store' });
        if (!f.ok) throw new Error('HTTP ' + f.status);
        const dec = window.LvBin.decodeToCanvas(await f.arrayBuffer());
        if (currentIconPath() !== rel) return;   // station changed mid-download
        // LV_IMAGE_ALIGN_STRETCH into a size x size object: the .bin is loaded at
        // up to 64x64 and then stretched to the configured box, aspect ignored.
        el.style.backgroundImage = `url("${dec.canvas.toDataURL('image/png')}")`;
        el.style.backgroundSize  = '100% 100%';
        el.style.display = '';
    } catch (err) {
        // The widget logs "Cannot load %s" and simply shows nothing.
        console.warn('Station icon unavailable:', rel, err.message);
        el.style.backgroundImage = '';
        el.style.display = 'none';
        iconLoadedPath = null;        // retry if the station comes round again
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Touch hotspots — mirrors touch_hotspots_widget.c
// ─────────────────────────────────────────────────────────────────────────────

// Must match UI_TOUCH_HOTSPOT_COUNT in ui_profile.h.
const HOTSPOT_COUNT = 8;

// control_action_t. The widget skips anything outside PLAY_TOGGLE..OPEN_EQUALIZER,
// so the range doubles as the validity check.
const HOTSPOT_ACTIONS = [
    'Play / stop', 'Previous', 'Next', 'Volume −', 'Volume +',
    'Stop', 'Play / pause', 'Open playlist', 'Open SD browser', 'Open equalizer',
];

// What to send for a hotspot press, or null when the action cannot be driven
// remotely. Returns a JSON command from docs/ws_protocol.md.
//
// The obvious mapping — the semantic plain-text frames (`toggle`, `next`, `volp`)
// — is deliberately NOT used, for two reasons:
//
//   * they act on whatever source is playing, resolved by media_source_current(),
//     while a radio hotspot always drives the radio (control_action_execute passes
//     CONTROL_SOURCE_RADIO). With SD playing, `next` would skip a track instead of
//     a station.
//   * `volp`/`volm` step by 5, whereas the hotspot path steps by 2
//     (media_control.c, MEDIA_ACTION_VOLUME_UP).
//
// So each action is expressed as the explicit radio command that reproduces what
// media_control_execute(MEDIA_SOURCE_RADIO, …) does, arithmetic included.
function hotspotCommand(action) {
    const L = S.live;
    const n = S.playlist.length;
    const idx = L.curr_index | 0;
    const playing = L.radio === 'playing';       // BUFFERING is not PLAYING, as in C

    switch (action) {
        case 0:   // PLAY_TOGGLE
        case 6:   // PLAY_PAUSE — "a stream can't pause; same as play/stop"
            return playing ? { cmd: 'stop' } : { cmd: 'play_index', index: idx };
        case 1:   // PREVIOUS
            return n > 0 ? { cmd: 'play_index', index: (idx - 1 + n) % n } : null;
        case 2:   // NEXT
            return n > 0 ? { cmd: 'play_index', index: (idx + 1) % n } : null;
        case 3:   // VOLUME_DOWN
            return { cmd: 'set_volume', value: clamp((L.volume | 0) - 2, 0, 100) };
        case 4:   // VOLUME_UP
            return { cmd: 'set_volume', value: clamp((L.volume | 0) + 2, 0, 100) };
        case 5:   // STOP — the JSON form stops the radio specifically
            return { cmd: 'stop' };
        default:
            // OPEN_PLAYLIST / OPEN_SD_BROWSER / OPEN_EQUALIZER navigate the panel's
            // own UI. `set_screen` only reaches radio/home/bt, so there is no frame
            // that can do this — the hotspot stays inert rather than doing something
            // almost-but-not-quite right.
            return null;
    }
}

function wsSend(obj) {
    if (!S.ws || S.ws.readyState !== 1) return false;
    S.ws.send(JSON.stringify(obj));
    return true;
}

// The hotspots draw nothing at rest — the wallpaper supplies the artwork and the
// button is a bare touch area over it. They are still built here, because their
// pressed highlight is real: hovering shows exactly what a finger sees, which is
// the only way to check a hotspot against the button painted into the wallpaper.
function renderHotspots(frag) {
    const p = S.p;
    for (let i = 1; i <= HOTSPOT_COUNT; i++) {
        const key = `radio_hotspot_${i}`;
        const w = p[`${key}_w`] | 0, h = p[`${key}_h`] | 0;
        const action = p[`${key}_action`] | 0;
        // Same rejection as the widget: disabled, degenerate, or an action outside
        // the enum means no object is created at all.
        if (!p[`${key}_enabled`] || w <= 0 || h <= 0 ||
            action < 0 || action >= HOTSPOT_ACTIONS.length) continue;

        const el = box(p[`${key}_x`] | 0, p[`${key}_y`] | 0, w, h, {});
        el.className = 'hotspot';
        // (min(w, h) * clamp(radius, 0, 100)) / 200, integer division included.
        el.style.borderRadius =
            Math.floor((Math.min(w, h) * clamp(p[`${key}_radius`] | 0, 0, 100)) / 200) + 'px';
        // Navigation actions have no wire equivalent: shown, but not clickable.
        const inert = hotspotCommand(action) === null && action >= 7;
        if (inert) {
            el.classList.add('inert');
            el.title = `hotspot ${i}: ${HOTSPOT_ACTIONS[action]} — panel-only, ` +
                       `no WebSocket command can trigger it`;
        } else {
            el.title = `hotspot ${i}: ${HOTSPOT_ACTIONS[action]}`;
            el.addEventListener('click', () => {
                // Re-resolved at click time: the command depends on live state
                // (what is playing, current index, current volume).
                const frame = hotspotCommand(action);
                if (!frame) return;          // empty playlist — the device no-ops too
                if (!wsSend(frame)) setWsBadge('down', 'not sent — no connection');
            });
        }
        frag.appendChild(el);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Needle VU — mirrors vu_needle_widget.c
// ─────────────────────────────────────────────────────────────────────────────

const SVG_NS = 'http://www.w3.org/2000/svg';
const NEEDLE_SWEEP_DEG = 90;   // full deflection arc: -45° (rest) … +45°

// The needle position is the one thing here that cannot be truthful: the RMS tap
// feeding it lives in the DSP element on the device and is not published anywhere
// a browser could read. So the meters are drawn at a representative deflection —
// around two thirds of the arc, which is where the widget's own comment says the
// gamma curve parks the operating point on normal programme. L and R differ
// slightly because on real stereo material they always do.
const NEEDLE_DEMO_LEVEL = { l: 0.66, r: 0.58 };

// meter_create() + tip_for_level(), including their integer truncation — the
// pivot sits at the bottom centre and the needle is the longest one that still
// fits the rect at full deflection. Degenerate sizes are guarded exactly as the
// firmware guards them, because a fresh profile really does carry zeros.
function needleGeometry(w, h, level) {
    w = Math.max(w | 0, 20);
    h = Math.max(h | 0, 20);

    const pivX = Math.floor(w / 2);
    const pivY = h - 3;
    const lenH = pivY - 3;
    const lenW = Math.trunc((pivX - 3) / Math.sin(NEEDLE_SWEEP_DEG * 0.5 * Math.PI / 180));
    const len  = Math.max(Math.min(lenH, lenW), 4);

    // C truncates after adding 0.5f rather than rounding, which differs for the
    // negative x of a needle left of its pivot — so truncate here too.
    const a    = (level - 0.5) * NEEDLE_SWEEP_DEG * (Math.PI / 180);
    const tipX = pivX + Math.trunc(Math.sin(a) * len + 0.5);
    const tipY = pivY - Math.trunc(Math.cos(a) * len + 0.5);

    return { w, h, pivX, pivY, len, tipX, tipY };
}

function needleMeter(x, y, w, h, level) {
    const { pivX, pivY, tipX, tipY } = needleGeometry(w, h, level);
    w = Math.max(w | 0, 20);
    h = Math.max(h | 0, 20);

    const p      = S.p;
    const needle = col(p.radio_needle_color, S.pal.accent);

    const el = box(x | 0, y | 0, w, h, {});
    // An opaque plate unless the meter is transparent, in which case the wallpaper
    // shows through and IS the meter face — scale, markings and all.
    if (!p.radio_needle_transparent) {
        el.style.background = col(p.radio_needle_bg_color, S.pal.bg_primary);
    }

    const svg = document.createElementNS(SVG_NS, 'svg');
    svg.setAttribute('width', w);
    svg.setAttribute('height', h);
    svg.setAttribute('viewBox', `0 0 ${w} ${h}`);
    svg.style.display = 'block';

    // needle_draw_cb(): a 2 px line with rounded ends, plus a 7 px round cap over
    // the pivot. Nothing else — no dial, no ticks.
    const line = document.createElementNS(SVG_NS, 'line');
    line.setAttribute('x1', pivX); line.setAttribute('y1', pivY);
    line.setAttribute('x2', tipX); line.setAttribute('y2', tipY);
    line.setAttribute('stroke', needle);
    line.setAttribute('stroke-width', 2);
    line.setAttribute('stroke-linecap', 'round');
    svg.appendChild(line);

    const cap = document.createElementNS(SVG_NS, 'circle');
    cap.setAttribute('cx', pivX); cap.setAttribute('cy', pivY);
    cap.setAttribute('r', 3.5);
    cap.setAttribute('fill', needle);
    svg.appendChild(cap);

    el.appendChild(svg);
    return el;
}

// ─────────────────────────────────────────────────────────────────────────────
// Volume slider — mirrors vol_slider_widget.c
// ─────────────────────────────────────────────────────────────────────────────

// vol_to_travel(): the LVGL range stays 0..100; radio_volslider_vol_max only
// rescales what that travel means.
function volToTravel(vol) {
    const max = (S.p.radio_volslider_vol_max >= 1 && S.p.radio_volslider_vol_max <= 100)
        ? S.p.radio_volslider_vol_max : 100;
    return clamp(Math.floor((vol * 100 + Math.floor(max / 2)) / max), 0, 100);
}

async function renderVolSlider(parent) {
    const p = S.p;
    if (!p.radio_volslider_show) return;

    let x = p.radio_volslider_x | 0, y = p.radio_volslider_y | 0;
    let w = p.radio_volslider_w | 0, h = p.radio_volslider_h | 0;
    const vertical = !!p.radio_volslider_vertical;

    // LVGL 9.2 takes the drag axis from w >= h regardless of the orientation call,
    // so the firmware swaps a contradicting box (and nudges a square by 1 px).
    if (vertical !== (h > w)) { const t = w; w = h; h = t; }
    if (vertical && w >= h) w = h - 1;

    const knobOnly = !!p.radio_volslider_knob_only;
    const fill     = S.pal.accent;          // not BT here, so accent (apply_colors)
    const track    = S.pal.text_muted;

    // Track + indicator. radius 0 throughout — the firmware squares the corners
    // deliberately (a CIRCLE-radius knob hangs the SW renderer at large sizes).
    const trackEl = box(x, y, w, h, {
        background: knobOnly ? 'transparent' : track,
    });
    parent.appendChild(trackEl);

    const travel = volToTravel(S.live.volume | 0);
    if (!knobOnly) {
        const indEl = vertical
            ? box(x, y + h - Math.round(h * travel / 100), w, Math.round(h * travel / 100),
                  { background: fill })
            : box(x, y, Math.round(w * travel / 100), h, { background: fill });
        indEl.dataset.role = 'vol_indicator';
        parent.appendChild(indEl);
        S.els.volIndicator = indEl;
    }

    // Knob artwork, when the slot holds an SD .bin. build_knob_image() sizes it
    // from the cross axis and lets the other axis follow the aspect ratio.
    const ref = String(p.radio_volslider_knob_image || '').trim();
    let knobW = vertical ? w : h, knobH = vertical ? w : h, knobUrl = '';

    if (ref && !/^asset\d$/.test(ref)) {
        try {
            const rel = ref.startsWith('/sdcard/') ? ref.slice('/sdcard'.length) : ref;
            const f = await fetch('/api/sd/file?path=' + encodeURIComponent(rel), { cache: 'no-store' });
            if (f.ok) {
                const dec = window.LvBin.decodeToCanvas(await f.arrayBuffer());
                if (vertical) { knobW = w; knobH = Math.max(1, Math.floor(dec.h * knobW / dec.w)); }
                else          { knobH = h; knobW = Math.max(1, Math.floor(dec.w * knobH / dec.h)); }
                knobUrl = dec.canvas.toDataURL('image/png');
            }
        } catch { /* unreadable artwork — the device keeps its plain themed knob */ }
    }

    const knobEl = box(0, 0, knobW, knobH, knobUrl
        ? { backgroundImage: `url("${knobUrl}")`, backgroundSize: '100% 100%' }
        : { background: fill });
    parent.appendChild(knobEl);

    // position_knob(): the knob travels inside the track, v=100 → right / top.
    S.els.volKnob = knobEl;
    S.els.volGeom = { x, y, w, h, knobW, knobH, vertical };
    positionKnob();
}

function positionKnob() {
    const g = S.els.volGeom;
    if (!g || !S.els.volKnob) return;
    const v  = volToTravel(S.live.volume | 0);
    const tx = Math.max(g.w - g.knobW, 0);
    const ty = Math.max(g.h - g.knobH, 0);
    const kx = g.vertical ? g.x + Math.floor((g.w - g.knobW) / 2)
                          : g.x + Math.floor(tx * v / 100);
    const ky = g.vertical ? g.y + Math.floor(ty * (100 - v) / 100)
                          : g.y + Math.floor((g.h - g.knobH) / 2);
    S.els.volKnob.style.left = kx + 'px';
    S.els.volKnob.style.top  = ky + 'px';

    const ind = S.els.volIndicator;
    if (ind) {
        if (g.vertical) {
            const hh = Math.round(g.h * v / 100);
            ind.style.top = (g.y + g.h - hh) + 'px';
            ind.style.height = hh + 'px';
        } else {
            ind.style.width = Math.round(g.w * v / 100) + 'px';
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Screen
// ─────────────────────────────────────────────────────────────────────────────

// Children are appended in screen_radio.c's creation order, so anything that
// overlaps stacks here the way it stacks on the panel.
async function renderScreen() {
    const p    = S.p;
    const th   = S.pal;
    const opa  = clamp(p.radio_label_bg_opa ?? 50, 0, 100);
    const frag = document.createDocumentFragment();

    S.els = {};

    // Cassette / rim wheels — animated artwork, not drawn yet.
    if (p.radio_show_cassette) {
        if (p.radio_show_wheel_left)
            frag.appendChild(stub(p.radio_cassette_l_x, p.radio_cassette_l_y,
                                  p.radio_cassette_l_size, p.radio_cassette_l_size, 'wheel L'));
        if (p.radio_show_wheel_right)
            frag.appendChild(stub(p.radio_cassette_r_x, p.radio_cassette_r_y,
                                  p.radio_cassette_r_size, p.radio_cassette_r_size, 'wheel R'));
    }

    // now_playing_widget: two independent boxed lines, each centre-anchored on
    // its own box (make_line: x + w / 2).
    if (p.radio_show_np) {
        const stationW = Math.max(p.radio_np_w | 0, 8);
        S.els.station = makeLabel({
            x: (p.radio_np_x | 0) + stationW / 2, y: p.radio_np_y | 0,
            fontId: p.radio_np_station_font, text: '', boxW: stationW, plate: opa,
            color: col(p.radio_np_color, th.accent),
        });
        frag.appendChild(S.els.station);

        if (p.radio_show_np_title) {
            const titleW = Math.max(p.radio_title_w | 0, 8);
            S.els.title = makeLabel({
                x: (p.radio_title_x | 0) + titleW / 2, y: p.radio_title_y | 0,
                fontId: p.radio_np_title_font, text: '', boxW: titleW, plate: opa,
                color: col(p.radio_title_color, th.text_secondary),
            });
            frag.appendChild(S.els.title);
        }
    }

    if (p.radio_show_station_icon) {
        // station_icon_widget_create() clamps the box to 16..64 px and starts hidden.
        const size = clamp(p.radio_station_icon_size | 0, 16, 64);
        S.els.stationIcon = box(p.radio_station_icon_x | 0, p.radio_station_icon_y | 0,
                                size, size, { display: 'none' });
        iconLoadedPath = null;        // this element has no artwork yet
        frag.appendChild(S.els.stationIcon);
    }
    if (p.radio_show_mode_indicator)
        frag.appendChild(stub(p.radio_mode_indic_x, p.radio_mode_indic_y, 16, 16, 'mode'));

    if (p.radio_show_clock) {
        S.els.clock = makeLabel({
            x: p.radio_clock_widget_x | 0, y: p.radio_clock_widget_y | 0,
            fontId: p.radio_clock_font, text: nowString(), plate: opa,
            color: th.text_primary,
        });
        frag.appendChild(S.els.clock);
    }

    if (p.radio_show_event_indicator)
        frag.appendChild(stub(p.radio_event_indic_x, p.radio_event_indic_y, 16, 16, 'event'));
    if (p.radio_show_vu)
        frag.appendChild(stub(p.radio_vu_x, p.radio_vu_y, p.radio_vu_w, p.radio_vu_h, 'VU'));
    if (p.radio_needle_show_l)
        frag.appendChild(needleMeter(p.radio_needle_l_x, p.radio_needle_l_y,
                                     p.radio_needle_l_w, p.radio_needle_l_h,
                                     NEEDLE_DEMO_LEVEL.l));
    if (p.radio_needle_show_r)
        frag.appendChild(needleMeter(p.radio_needle_r_x, p.radio_needle_r_y,
                                     p.radio_needle_r_w, p.radio_needle_r_h,
                                     NEEDLE_DEMO_LEVEL.r));
    if (p.radio_stereo_show_l)
        frag.appendChild(stub(p.radio_stereo_l_x, p.radio_stereo_l_y,
                              p.radio_stereo_l_w, p.radio_stereo_l_h, 'bar L'));
    if (p.radio_stereo_show_r)
        frag.appendChild(stub(p.radio_stereo_r_x, p.radio_stereo_r_y,
                              p.radio_stereo_r_w, p.radio_stereo_r_h, 'bar R'));

    // weather_widget centres an icon + text pill inside a full-width frame. The
    // pill is drawn, the icon is not (its glyphs live in lv_font_weather_20), and
    // the reading is sample text — the device does not serve current conditions.
    if (p.radio_show_weather) {
        const wSpan = (p.radio_weather_w | 0) > 0 ? (p.radio_weather_w | 0) : S.meta.screen_w;
        frag.appendChild(makeLabel({
            x: (p.radio_weather_x | 0) + wSpan / 2, y: p.radio_weather_y | 0,
            fontId: p.radio_weather_font, text: '+21°C  54%', plate: opa,
            color: th.text_primary,
        }));
    }

    if (p.radio_show_playback_status) {
        S.els.state = makeLabel({
            x: p.radio_state_x | 0, y: p.radio_state_y | 0,
            fontId: p.radio_state_font, text: '', plate: opa,
            color: col(p.radio_state_color, th.status_ok),
        });
        frag.appendChild(S.els.state);
    }

    // make_info_label(): four independent centre-anchored labels sharing one font
    // and one colour override.
    const infoColor = col(p.radio_info_color, th.text_muted);
    const info = (key, xf, yf) => {
        S.els[key] = makeLabel({
            x: p[xf] | 0, y: p[yf] | 0, fontId: p.radio_audio_info_font,
            text: '', plate: opa, color: infoColor,
        });
        frag.appendChild(S.els[key]);
    };
    if (p.radio_samplerate_show) info('samplerate', 'radio_samplerate_x', 'radio_samplerate_y');
    if (p.radio_channels_show)   info('channels',   'radio_channels_x',   'radio_channels_y');
    if (p.radio_bitrate_show)    info('bitrate',    'radio_bitrate_x',    'radio_bitrate_y');
    if (p.radio_volume_show)     info('volume',     'radio_volume_x',     'radio_volume_y');

    // Created after everything else and before the slider, exactly as radio_create()
    // orders it — a hotspot laid over the slider must not shadow it.
    renderHotspots(frag);

    screenEl.replaceChildren(frag);

    // The tap-to-show controls overlay is intentionally absent: it only exists once
    // the user touches the screen, so drawing it at rest would misrepresent the panel.

    await renderVolSlider(screenEl);
    refreshLive();
}

// ─────────────────────────────────────────────────────────────────────────────
// Live data
// ─────────────────────────────────────────────────────────────────────────────

// The device clock is not in the state broadcast, so this is the browser's time —
// same format (display.time_ampm), possibly a different second.
function nowString() {
    const d = new Date();
    const ampm = !!(S.settings?.display?.time_ampm);
    let h = d.getHours();
    const m = String(d.getMinutes()).padStart(2, '0');
    if (!ampm) return String(h).padStart(2, '0') + ':' + m;
    const suffix = h >= 12 ? ' PM' : ' AM';
    h = h % 12 || 12;
    return h + ':' + m + suffix;
}

// radio_state_str(): FINISHED shows as STOPPED, everything else is the state
// spelled in capitals.
function stateText(s) {
    const map = {
        stopped: 'STOPPED', playing: 'PLAYING', buffering: 'BUFFERING',
        error: 'ERROR', finished: 'STOPPED',
    };
    return map[s] || '?';
}

// refresh_from_state(): the metadata labels stay empty (and therefore hidden)
// until the decoder reports a valid stream; volume is always meaningful.
function refreshLive() {
    const L = S.live;

    setLabelText(S.els.station, L.station_name || 'Atlas Radio');
    // The shared title field also carries the SD track, so radio only shows it
    // while radio is the active source.
    if (S.els.title) setLabelText(S.els.title, (!L.sd_active && L.title) ? L.title : '');

    if (S.els.state) setLabelText(S.els.state, stateText(L.radio));
    if (S.els.clock) setLabelText(S.els.clock, nowString());

    const sr = L.sr | 0, br = L.br | 0;
    if (S.els.samplerate) setLabelText(S.els.samplerate, sr > 0 ? `${sr} Hz` : '');
    if (S.els.channels)   setLabelText(S.els.channels, sr <= 0 ? '' : ((L.ch | 0) === 1 ? 'MONO' : 'STEREO'));
    if (S.els.bitrate)    setLabelText(S.els.bitrate, (sr > 0 && br > 0) ? `${Math.round(br / 1000)} kbps` : '');
    if (S.els.volume)     setLabelText(S.els.volume, `VOL: ${L.volume | 0}%`);

    positionKnob();
    refreshStationIcon();   // async; no-op unless the station actually changed
}

function setWsBadge(cls, text) {
    const b = document.getElementById('ws_badge');
    b.className = 'badge' + (cls ? ' ' + cls : '');
    b.textContent = text;
}

function connectWs() {
    S.ws = new WebSocket(`ws://${location.host}/ws`);

    S.ws.onopen = () => { S.wsRetry = 250; setWsBadge('live', 'live'); };
    S.ws.onerror = () => setWsBadge('down', 'ws error');
    S.ws.onclose = () => {
        const delay = S.wsRetry;
        S.wsRetry = Math.min(S.wsRetry * 2, 3000);
        setWsBadge('down', 'reconnecting…');
        setTimeout(connectWs, delay);
    };
    S.ws.onmessage = (msg) => {
        let d;
        try { d = JSON.parse(msg.data); } catch { return; }
        if (d.type !== 'state') return;
        Object.assign(S.live, {
            radio: d.radio ?? S.live.radio,
            station_name: d.station_name ?? S.live.station_name,
            title: d.title ?? S.live.title,
            volume: d.volume ?? S.live.volume,
            sr: d.sr ?? S.live.sr,
            ch: d.ch ?? S.live.ch,
            br: d.br ?? S.live.br,
            sd_active: d.sd_active ?? S.live.sd_active,
            curr_index: d.curr_index ?? S.live.curr_index,
        });
        refreshLive();
    };
}

// ─────────────────────────────────────────────────────────────────────────────
// Zoom
// ─────────────────────────────────────────────────────────────────────────────

function applyZoom() {
    const mode = document.getElementById('zoom').value;
    const w = S.meta.screen_w, h = S.meta.screen_h;
    // Measure the page, not the frame: the frame is inline-block and sized BY the
    // stage, so asking it how wide it is would just echo the previous zoom back.
    const avail = document.body.clientWidth - 62;   // body padding + frame padding/border
    const k = mode === 'fit' ? clamp(avail / w, 1, 4) : parseFloat(mode);
    screenEl.style.transform = `scale(${k})`;
    stageEl.style.width  = Math.round(w * k) + 'px';
    stageEl.style.height = Math.round(h * k) + 'px';
}

// ─────────────────────────────────────────────────────────────────────────────
// Boot
// ─────────────────────────────────────────────────────────────────────────────

function fail(msg) {
    const el = document.getElementById('err');
    el.textContent = msg;
    el.style.display = 'block';
}

async function loadAll() {
    const get = async (url) => {
        const r = await fetch(url, { cache: 'no-store' });
        if (!r.ok) throw new Error(url + ' → HTTP ' + r.status);
        return r.json();
    };
    const [meta, radio, settings, theme] = await Promise.all([
        get('/api/ui/profile/meta'),
        get('/api/ui/profile/radio'),
        get('/api/settings'),
        get('/api/theme'),
    ]);
    S.meta     = meta;
    S.p        = radio;
    S.settings = settings;
    S.pal      = theme[theme.current] || theme.dark;

    // Kept out of the group above on purpose: the station list is only needed for
    // the icon, so a playlist that fails to load must not take the screen with it.
    try {
        const pl = await get('/api/playlist');
        S.playlist = Array.isArray(pl) ? pl : [];
    } catch (err) {
        S.playlist = [];
        console.warn('Playlist unavailable — station icon disabled:', err.message);
    }

    screenEl.style.width  = meta.screen_w + 'px';
    screenEl.style.height = meta.screen_h + 'px';
}

async function boot() {
    try {
        await loadAll();
        // Measure the webfont, never the fallback: the baseline correction is only
        // meaningful once the real glyph metrics are in.
        await document.fonts.load('500 16px AtlasMontserrat');
        await document.fonts.ready;
        baselineFix.clear();

        applyZoom();
        await renderScreen();
        await applyBackground();
        connectWs();
        setInterval(() => { if (S.els.clock) setLabelText(S.els.clock, nowString()); }, 10000);
    } catch (err) {
        fail('Could not load the screen: ' + err.message);
    }
}

document.getElementById('zoom').addEventListener('change', applyZoom);
document.getElementById('show_hotspots').addEventListener('change', (e) => {
    screenEl.classList.toggle('show-hotspots', e.target.checked);
});
window.addEventListener('resize', () => { if (S.meta) applyZoom(); });
document.getElementById('reload').addEventListener('click', async () => {
    try {
        await loadAll();
        baselineFix.clear();
        applyZoom();
        await renderScreen();
        await applyBackground();
    } catch (err) {
        fail('Reload failed: ' + err.message);
    }
});

window.addEventListener('DOMContentLoaded', boot);
