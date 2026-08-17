'use strict';

// Browser rendering of the device's LVGL screens, as the panel actually shows them.
//
// Every number comes from the device, none of it is duplicated here:
//   /api/ui/profile/meta      — panel size + real LVGL line_height/base_line per font
//   /api/ui/profile/<section> — the geometry each screen_*.c builds from
//   /api/theme                — active palette (a profile colour of 0 inherits it)
//   /api/settings             — background tier (wallpaper / gradient / solid) + dim
//   /api/playlist             — station list (names, favourites, icons)
//   ws://<host>/ws            — live state, and the channel controls write back on
//
// What IS restated here are the firmware's drawing rules — where a centre-anchored
// label lands, how the scrim plate pads it, how the background tiers resolve. Each
// of those names the C function it mirrors, so the two can be checked against each
// other when a layout rule changes.
//
// This file holds everything screen-independent plus the widgets that several
// screens share (they take a profile section prefix): needle VU, volume slider,
// touch hotspots, clock, indicators, placeholders. The per-screen renderers live in
// preview-radio.js, preview-sd.js and preview-list.js.

// ─────────────────────────────────────────────────────────────────────────────
// State
// ─────────────────────────────────────────────────────────────────────────────

const S = {
    meta:     null,   // /api/ui/profile/meta
    prof:     {},     // section name → /api/ui/profile/<section>
    settings: null,
    pal:      null,   // active palette from /api/theme
    playlist: [],     // /api/playlist
    sdList:   null,   // last type:"sd_list" broadcast
    mode:     'radio',
    scale:    1,      // current zoom; drag-scroll converts page px back through it
    gotState: false,
    live: {
        radio: 'stopped', station_name: '', title: '', volume: 0,
        sr: 0, ch: 2, br: 0, curr_index: -1,
        sd_active: false, sd_index: 0, sd_count: 0, sd_track: '', sd_dir: '',
        sd_paused: false, sd_shuffle: false, sd_repeat: 0,
        sd_position_ms: null, sd_duration_ms: null,   // null = firmware predates them
        bt_state: 1, bt_playing: false, bt_volume: 0, bt_title: '', bt_artist: '',
        bt_duration_ms: 0, bt_position_s: 0,
        eq: [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],   // the 10 band gains, dB
    },
    sdPosAt: 0,       // performance.now() when sd_position_ms was received
    els: {},          // live-updating nodes of the primary screen
    ws: null,
    wsRetry: 250,
};

// Each mode pairs a "what is playing" screen with the list you pick from — the
// same pairing the device makes between a source and its browser. BT has no list
// of its own (the phone owns the queue), so its `list` is null and every list
// step is skipped rather than faked.
const MODES = {
    radio: { primary: 'radio', list: 'playlist', listCaption: 'Playlist screen' },
    sd:    { primary: 'sd',    list: 'browser',  listCaption: 'SD browser screen' },
    bt:    { primary: 'bt',    list: null,       listCaption: '' },
};

// Which source was shown last, so a reload comes back where it left off. A view
// preference of this browser, nothing the device knows about. Validated against
// MODES, so a key left over from a build that had a source this one does not
// falls back instead of rendering nothing.
const MODE_KEY = 'atlascube.preview.mode';

function storedMode() {
    try {
        const v = localStorage.getItem(MODE_KEY);
        if (MODES[v]) return v;
    } catch (e) { /* private mode */ }
    return 'radio';
}

const screenEl   = document.getElementById('screen');
const stageEl    = document.getElementById('stage');
const frameEl    = document.querySelector('.frame');
const viewerEl   = document.getElementById('viewer');
const volbarEl   = document.getElementById('volbar');
const listWrapEl = document.getElementById('list_wrap');
const listStage  = document.getElementById('list_stage');
const listScreen = document.getElementById('list_screen');
const listCapEl  = document.getElementById('list_caption');

function modeCfg() { return MODES[S.mode]; }
function primaryProfile() { return S.prof[modeCfg().primary]; }
function listProfile() { return S.prof[modeCfg().list]; }

// ─────────────────────────────────────────────────────────────────────────────
// Colours
// ─────────────────────────────────────────────────────────────────────────────

// A ui_profile colour field is a packed RGB integer where 0 means "inherit the
// theme", exactly as every `p->..._color ? … : th->…` in the screens reads it.
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

// "m:ss", the format both players print — the minutes are not zero-padded, and a
// position that has not started yet reads 0:00 rather than a negative time.
function fmtMmss(ms) {
    const s = Math.max(Math.floor(ms / 1000), 0);
    return `${Math.floor(s / 60)}:${String(s % 60).padStart(2, '0')}`;
}

// ─────────────────────────────────────────────────────────────────────────────
// Fonts
// ─────────────────────────────────────────────────────────────────────────────

// Nominal pixel size out of the font id ("montserrat_18_eu" → 18). It is the em
// size lv_font_conv was given, which is also what CSS font-size means, so the
// glyphs come out at the same scale. The suffix is matched loosely on purpose:
// older firmware ships the same fonts as "_pl".
function fontPx(id) {
    const m = String(id || '').match(/_(\d+)(_[a-z]+)?$/);
    return m ? parseInt(m[1], 10) : 14;
}

// Box height and baseline the device uses. api_ui_profile_meta_get_handler sends
// the real lv_font_t line_height/base_line; the fallbacks are the ratios they
// average out to, for firmware predating the field.
function lvMetrics(id) {
    const px = fontPx(id);
    const m  = (S.meta.font_metrics || {})[id];
    const lh = m ? m.h : Math.round(px * 1.09);
    const baseFromTop = m ? m.h - m.b : Math.round(lh * 0.82);
    return { px, lh, baseFromTop };
}

// Where the browser would put the baseline inside a line box of `lh` px: CSS
// splits the leftover leading evenly above and below the font's ascent+descent.
const measureCtx = document.createElement('canvas').getContext('2d');

function browserBaseline(px, lh) {
    measureCtx.font = `500 ${px}px AtlasMontserrat`;
    const tm = measureCtx.measureText('Hxg');
    const a = tm.fontBoundingBoxAscent, d = tm.fontBoundingBoxDescent;
    if (!(a > 0)) return lh * 0.82;
    return (lh - (a + d)) / 2 + a;
}

// Per-font nudge that lands the browser's baseline exactly on the device's.
// Measured rather than assumed — without it the text sits a pixel or two off and
// every judgement about the layout would be made against a lie.
const baselineFix = new Map();

function baselineOffset(id) {
    if (!baselineFix.has(id)) {
        const { px, lh, baseFromTop } = lvMetrics(id);
        baselineFix.set(id, baseFromTop - browserBaseline(px, lh));
    }
    return baselineFix.get(id);
}

// ─────────────────────────────────────────────────────────────────────────────
// Primitives
// ─────────────────────────────────────────────────────────────────────────────

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

// One label as ui_anchored_label() + ui_label_scrim() produce it.
//
//   align 'center' → x is the horizontal middle of the object (ui_label.c
//                    on_size_changed: x -= w / 2, width including padding, which
//                    is what translateX(-50%) does over the border box)
//   boxW           → ui_label_set_text_boxed(): the label hugs its text but never
//                    grows past the box; text centred inside it
//   plate          → the section's label_bg_opa. ui_label_scrim() returns early at
//                    0, so at 0 there is no padding either.
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

    const span = document.createElement('span');
    span.style.position = 'relative';
    span.style.top      = baselineOffset(fontId).toFixed(2) + 'px';
    span.textContent    = text;
    el.appendChild(span);
    el._span = span;
    return el;
}

// ui_label_set_text(): empty text hides the whole label, so no empty plate shows.
function setLabelText(el, text) {
    if (!el) return;
    el._span.textContent = text;
    el.style.display = text ? '' : 'none';
}

// A left- or right-positioned single line, used where a label sits inside a strip
// or a list row rather than being centre-anchored on a point.
function alignedText({ text, fontId, color, left, top }) {
    const { px, lh } = lvMetrics(fontId);
    const el = document.createElement('div');
    el.style.position   = 'absolute';
    el.style.top        = top + 'px';
    if (left !== undefined) el.style.left = left + 'px';
    el.style.fontSize   = px + 'px';
    el.style.lineHeight = lh + 'px';
    el.style.height     = lh + 'px';
    el.style.color      = color;
    el.style.whiteSpace = 'pre';    // list rows are column-aligned with spaces
    el.style.overflow   = 'hidden';

    const span = document.createElement('span');
    span.style.position = 'relative';
    span.style.top      = baselineOffset(fontId).toFixed(2) + 'px';
    span.textContent    = text;
    el.appendChild(span);
    return el;
}

// Decoded artwork goes into the page as a <canvas>, never as a PNG data URL:
// Chromium on Android has been seen returning a blank (white) image from
// canvas.toDataURL() for artwork with an alpha channel — Edge/Android drew the
// knobs as white rectangles while the very same canvas inserted into the DOM drew
// correctly. Stretched to the element's box, which is LV_IMAGE_ALIGN_STRETCH, the
// alignment the widgets use. The copy is needed because one canvas cannot be in
// two places at once — the ten EQ bands share a single decoded knob.
function paintArt(el, src) {
    const c = document.createElement('canvas');
    c.width  = src.width;
    c.height = src.height;
    c.getContext('2d').drawImage(src, 0, 0);
    c.style.position = 'absolute';
    c.style.left   = '0';
    c.style.top    = '0';
    c.style.width  = '100%';
    c.style.height = '100%';
    el.replaceChildren(c);
}

function clearArt(el) { el.replaceChildren(); }

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

// ─────────────────────────────────────────────────────────────────────────────
// Background — mirrors ui_background_apply()
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
    return dimPct > 0
        ? `linear-gradient(rgba(0,0,0,${dimPct / 100}), rgba(0,0,0,${dimPct / 100}))` : '';
}

// `ovr` is the screen's own wallpaper field — every hub screen has one, and they
// resolve identically; only the field and the element painted differ.
async function applyBackground(targetEl, ovr) {
    const display = S.settings.display || {};
    const dim     = clamp(display.wallpaper_dim || 0, 0, 100);
    ovr           = String(ovr || '');
    const slot    = netSlotOf(ovr);
    const isPath  = ovr && ovr !== 'none' && slot < 0;

    const paint = (image) => {
        const layers = [dimLayer(dim), image].filter(Boolean).join(', ');
        targetEl.style.background     = layers || S.pal.bg_primary;
        targetEl.style.backgroundSize = 'cover';
    };

    // 1. Internet wallpaper — only when this screen is not pinned to an SD file.
    if (!isPath) {
        try {
            const st   = await fetch('/api/wallpaper/status', { cache: 'no-store' });
            const info = st.ok ? await st.json() : null;
            if (info && info.active) {
                const img = await fetch('/api/wallpaper/image?slot=' + (slot >= 0 ? slot : 0),
                                        { cache: 'no-store' });
                if (img.ok) {
                    const dec = window.LvBin.decodeToCanvas(await img.arrayBuffer());
                    paint(`url("${dec.canvas.toDataURL('image/png')}")`);
                    return;
                }
            }
        } catch { /* no fetched wallpaper — fall through to the tiers below */ }
    }

    // 2. An explicit per-screen SD .bin. Note there is no global-wallpaper tier
    //    for the hub screens: screen_wp_override() returns their own field (never
    //    NULL), so the firmware's `!ovr` fallback to display.wallpaper_path cannot
    //    apply here.
    if (isPath) {
        try {
            const rel = ovr.startsWith('/sdcard/') ? ovr.slice('/sdcard'.length) : ovr;
            const f = await fetch('/api/sd/file?path=' + encodeURIComponent(rel),
                                  { cache: 'no-store' });
            if (f.ok) {
                const dec = window.LvBin.decodeToCanvas(await f.arrayBuffer());
                paint(`url("${dec.canvas.toDataURL('image/png')}")`);
                return;
            }
        } catch { /* unreadable file — the device falls back to the gradient too */ }
    }

    // 3. Solid, when the gradient is switched off.
    if (!display.bg_gradient) {
        targetEl.style.background = S.pal.bg_primary;
        return;
    }

    // 4. Vertical palette gradient (the device dithers it into RGB565; the browser
    //    renders it in full colour, so banding differs — the colours do not).
    targetEl.style.background =
        `linear-gradient(${S.pal.bg_grad_top}, ${S.pal.bg_grad_bottom})`;
}

// ─────────────────────────────────────────────────────────────────────────────
// Shared widgets — all take the profile section prefix
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

// meter_create() + tip_for_level(), including their integer truncation.
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

function needleMeter(p, pre, x, y, w, h, level) {
    const g = needleGeometry(w, h, level);
    const needle = col(p[`${pre}_needle_color`], S.pal.accent);

    const el = box(x | 0, y | 0, g.w, g.h, {});
    // An opaque plate unless the meter is transparent, in which case the wallpaper
    // shows through and IS the meter face — scale, markings and all.
    if (!p[`${pre}_needle_transparent`]) {
        el.style.background = col(p[`${pre}_needle_bg_color`], S.pal.bg_primary);
    }

    const svg = document.createElementNS(SVG_NS, 'svg');
    svg.setAttribute('width', g.w);
    svg.setAttribute('height', g.h);
    svg.setAttribute('viewBox', `0 0 ${g.w} ${g.h}`);
    svg.style.display = 'block';

    // needle_draw_cb(): a 2 px line with rounded ends, plus a 7 px round cap over
    // the pivot. Nothing else — no dial, no ticks.
    const line = document.createElementNS(SVG_NS, 'line');
    line.setAttribute('x1', g.pivX); line.setAttribute('y1', g.pivY);
    line.setAttribute('x2', g.tipX); line.setAttribute('y2', g.tipY);
    line.setAttribute('stroke', needle);
    line.setAttribute('stroke-width', 2);
    line.setAttribute('stroke-linecap', 'round');
    svg.appendChild(line);

    const cap = document.createElementNS(SVG_NS, 'circle');
    cap.setAttribute('cx', g.pivX); cap.setAttribute('cy', g.pivY);
    cap.setAttribute('r', 3.5);
    cap.setAttribute('fill', needle);
    svg.appendChild(cap);

    el.appendChild(svg);
    return el;
}

// Cassette reel / car rim. Unlike every other widget here this one CANNOT be
// mirrored: animated_wheels_widget.c is a thin adapter over a prebuilt static
// library, so the drawing code is not readable. What is known from the ABI is the
// geometry (a square of `size`) and the four palette entries it is handed — so the
// mark below is an approximation built from those, not a copy of the artwork.
// Kept deliberately faint: it says "a wheel belongs here", it does not claim to be
// the wheel. Style 1 (car rims) gets more spokes than style 0 (cassette reels).
function wheelMark(p, pre, x, y, size) {
    size = Math.max(size | 0, 16);
    const c = size / 2;
    const el = box(x | 0, y | 0, size, size, { opacity: '.7' });

    const svg = document.createElementNS(SVG_NS, 'svg');
    svg.setAttribute('width', size);
    svg.setAttribute('height', size);
    svg.setAttribute('viewBox', `0 0 ${size} ${size}`);
    svg.style.display = 'block';

    const circle = (r, fill, stroke, width) => {
        const el = document.createElementNS(SVG_NS, 'circle');
        el.setAttribute('cx', c); el.setAttribute('cy', c); el.setAttribute('r', r);
        el.setAttribute('fill', fill || 'none');
        if (stroke) { el.setAttribute('stroke', stroke); el.setAttribute('stroke-width', width); }
        return el;
    };

    const rim = c - 1.5;
    svg.appendChild(circle(rim, null, S.pal.text_muted, 2));

    const spokes = (p[`${pre}_animation_style`] | 0) === 1 ? 5 : 3;
    const hubR   = Math.max(size * 0.18, 3);
    for (let i = 0; i < spokes; i++) {
        const a = (i / spokes) * Math.PI * 2 - Math.PI / 2;
        const line = document.createElementNS(SVG_NS, 'line');
        line.setAttribute('x1', c + Math.cos(a) * hubR);
        line.setAttribute('y1', c + Math.sin(a) * hubR);
        line.setAttribute('x2', c + Math.cos(a) * (rim - 1));
        line.setAttribute('y2', c + Math.sin(a) * (rim - 1));
        line.setAttribute('stroke', S.pal.text_secondary);
        line.setAttribute('stroke-width', 2);
        line.setAttribute('stroke-linecap', 'round');
        svg.appendChild(line);
    }
    svg.appendChild(circle(hubR, S.pal.accent, null, 0));

    el.appendChild(svg);
    return el;
}

// Spectrum VU, stereo bars, mode/event indicators — audio-driven or animated
// artwork the preview does not draw — plus the wheels and the needle meters.
function renderSharedStubs(frag, p, pre) {
    if (p[`${pre}_show_cassette`]) {
        if (p[`${pre}_show_wheel_left`])
            frag.appendChild(wheelMark(p, pre, p[`${pre}_cassette_l_x`],
                                       p[`${pre}_cassette_l_y`], p[`${pre}_cassette_l_size`]));
        if (p[`${pre}_show_wheel_right`])
            frag.appendChild(wheelMark(p, pre, p[`${pre}_cassette_r_x`],
                                       p[`${pre}_cassette_r_y`], p[`${pre}_cassette_r_size`]));
    }
    if (p[`${pre}_show_mode_indicator`])
        frag.appendChild(stub(p[`${pre}_mode_indic_x`], p[`${pre}_mode_indic_y`], 16, 16, 'mode'));
    if (p[`${pre}_show_event_indicator`])
        frag.appendChild(stub(p[`${pre}_event_indic_x`], p[`${pre}_event_indic_y`], 16, 16, 'event'));
    if (p[`${pre}_show_vu`])
        frag.appendChild(stub(p[`${pre}_vu_x`], p[`${pre}_vu_y`],
                              p[`${pre}_vu_w`], p[`${pre}_vu_h`], 'VU'));
    if (p[`${pre}_stereo_show_l`])
        frag.appendChild(stub(p[`${pre}_stereo_l_x`], p[`${pre}_stereo_l_y`],
                              p[`${pre}_stereo_l_w`], p[`${pre}_stereo_l_h`], 'bar L'));
    if (p[`${pre}_stereo_show_r`])
        frag.appendChild(stub(p[`${pre}_stereo_r_x`], p[`${pre}_stereo_r_y`],
                              p[`${pre}_stereo_r_w`], p[`${pre}_stereo_r_h`], 'bar R'));

    if (p[`${pre}_needle_show_l`])
        frag.appendChild(needleMeter(p, pre, p[`${pre}_needle_l_x`], p[`${pre}_needle_l_y`],
                                     p[`${pre}_needle_l_w`], p[`${pre}_needle_l_h`],
                                     NEEDLE_DEMO_LEVEL.l));
    if (p[`${pre}_needle_show_r`])
        frag.appendChild(needleMeter(p, pre, p[`${pre}_needle_r_x`], p[`${pre}_needle_r_y`],
                                     p[`${pre}_needle_r_w`], p[`${pre}_needle_r_h`],
                                     NEEDLE_DEMO_LEVEL.r));
}

// clock_widget: an "HH:MM" label on the section's plate. The anchor is the
// screen's to choose, not the widget's — radio and SD pass UI_ALIGN_CENTER,
// screen_bt.c passes UI_ALIGN_LEFT, and centring a left-anchored clock would
// shift it half its own width off.
function renderClockWidget(frag, p, pre, align = 'center') {
    if (!p[`${pre}_show_clock`]) return;
    S.els.clock = makeLabel({
        x: p[`${pre}_clock_widget_x`] | 0, y: p[`${pre}_clock_widget_y`] | 0,
        fontId: p[`${pre}_clock_font`], text: nowString(), align,
        plate: clamp(p[`${pre}_label_bg_opa`] ?? 50, 0, 100),
        color: S.pal.text_primary,
    });
    frag.appendChild(S.els.clock);
}

// ── Volume slider (the one drawn ON the screen) — vol_slider_widget.c ────────

// The BT screen drives the module's own level (app_state.bt_volume, set through
// settings_set_bt_volume), every other source the engine's master volume — the
// same split vol_slider_widget.c makes on its s_bt flag. Two separate numbers on
// the device, so reading or writing the wrong one would move the wrong output.
function volumeOf(source = S.mode) {
    return (source === 'bt' ? S.live.bt_volume : S.live.volume) | 0;
}

function volumeCommand(value, source = S.mode) {
    return source === 'bt' ? { cmd: 'bt_volume', value } : { cmd: 'set_volume', value };
}

// vol_to_travel(): the LVGL range stays 0..100; <pre>_volslider_vol_max only
// rescales what that travel means.
function volSliderMax() {
    const p = primaryProfile();
    const raw = p ? p[`${modeCfg().primary}_volslider_vol_max`] : 100;
    return (raw >= 1 && raw <= 100) ? raw : 100;
}

function volToTravel(vol) {
    const max = volSliderMax();
    return clamp(Math.floor((vol * 100 + Math.floor(max / 2)) / max), 0, 100);
}

// travel_to_vol(): the other direction, for a finger that lands on the track.
function travelToVol(travel) {
    return clamp(Math.floor((clamp(travel, 0, 100) * volSliderMax() + 50) / 100), 0, 100);
}

async function renderVolSlider(parent, p, pre) {
    if (!p[`${pre}_volslider_show`]) return;

    let x = p[`${pre}_volslider_x`] | 0, y = p[`${pre}_volslider_y`] | 0;
    let w = p[`${pre}_volslider_w`] | 0, h = p[`${pre}_volslider_h`] | 0;
    const vertical = !!p[`${pre}_volslider_vertical`];

    // LVGL 9.2 takes the drag axis from w >= h regardless of the orientation call,
    // so the firmware swaps a contradicting box (and nudges a square by 1 px).
    if (vertical !== (h > w)) { const t = w; w = h; h = t; }
    if (vertical && w >= h) w = h - 1;

    const knobOnly = !!p[`${pre}_volslider_knob_only`];
    // apply_theme(): the BT screen paints its slider in the brand colour, the
    // others in the accent.
    const fill     = pre === 'bt' ? S.pal.bt_brand : S.pal.accent;
    const track    = S.pal.text_muted;

    // radius 0 throughout — the firmware squares the corners deliberately (a
    // CIRCLE-radius knob hangs the SW renderer at large sizes). The track is the
    // one element that takes the pointer (the fill and the knob are painted over
    // it and pass events through), so it exists even in knob-only mode, exactly
    // as the transparent LVGL slider underneath the artwork does.
    const trackEl = box(x, y, w, h, { background: knobOnly ? 'transparent' : track });
    parent.appendChild(trackEl);

    const travel = volToTravel(volumeOf());
    if (!knobOnly) {
        const indEl = vertical
            ? box(x, y + h - Math.round(h * travel / 100), w, Math.round(h * travel / 100),
                  { background: fill, pointerEvents: 'none' })
            : box(x, y, Math.round(w * travel / 100), h,
                  { background: fill, pointerEvents: 'none' });
        parent.appendChild(indEl);
        S.els.volIndicator = indEl;
    }

    // Knob artwork, when the slot holds an SD .bin. build_knob_image() sizes it
    // from the cross axis and lets the other axis follow the aspect ratio.
    const ref = String(p[`${pre}_volslider_knob_image`] || '').trim();
    let knobW = vertical ? w : h, knobH = vertical ? w : h, knobArt = null;

    if (ref && !/^asset\d$/.test(ref)) {
        try {
            const rel = ref.startsWith('/sdcard/') ? ref.slice('/sdcard'.length) : ref;
            const f = await fetch('/api/sd/file?path=' + encodeURIComponent(rel),
                                  { cache: 'no-store' });
            if (f.ok) {
                const dec = window.LvBin.decodeToCanvas(await f.arrayBuffer());
                if (vertical) { knobW = w; knobH = Math.max(1, Math.floor(dec.h * knobW / dec.w)); }
                else          { knobH = h; knobW = Math.max(1, Math.floor(dec.w * knobH / dec.h)); }
                knobArt = dec.canvas;
            }
        } catch { /* unreadable artwork — the device keeps its plain themed knob */ }
    }

    const knobEl = box(0, 0, knobW, knobH, knobArt
        ? { pointerEvents: 'none' } : { background: fill, pointerEvents: 'none' });
    if (knobArt) paintArt(knobEl, knobArt);
    parent.appendChild(knobEl);

    S.els.volKnob = knobEl;
    S.els.volGeom = { x, y, w, h, knobW, knobH, vertical };
    trackEl.style.cursor = vertical ? 'ns-resize' : 'ew-resize';
    bindVolSlider(trackEl);
    positionKnob();
}

// ── Dragging the on-screen slider ───────────────────────────────────────────
//
// The panel's slider is a real control, so this one is too. An LVGL slider jumps
// to the press point, which is why the value is set on pointerdown and not only
// while moving.
//
// When it applies differs by channel, and that difference is the firmware's:
// value_changed_cb() drives audio_engine_set_volume() live on the main channel
// but returns early for BT ("BT applies on release only"), and released_cb()
// commits through settings on both. So the main channel gets debounced frames
// during the drag — the same 150 ms the page's own bar uses, since each one is a
// settings write on the device — and BT gets nothing until the finger lifts.
let volKnobDragging = false;
let volKnobTimeout  = null;
// What the finger is pointing at. Sends read this rather than S.live, which a
// state broadcast can overwrite with the device's slightly older level between
// two moves — releasing would then commit the value we were dragging away from.
let volKnobTravel   = 0;

function volTravelAt(clientX, clientY) {
    const g = S.els.volGeom;
    const rect = stageEl.getBoundingClientRect();
    const lx = (clientX - rect.left) / S.scale;
    const ly = (clientY - rect.top)  / S.scale;
    const frac = g.vertical ? (g.y + g.h - ly) / g.h : (lx - g.x) / g.w;
    return clamp(Math.round(frac * 100), 0, 100);
}

// The knob follows the finger, and so does everything else reading the level —
// the page bar included, which is what the panel does too (the on-screen slider
// and the overlay show one number).
function volDragTo(travel) {
    volKnobTravel = travel;
    const vol = travelToVol(travel);
    if (S.mode === 'bt') S.live.bt_volume = vol;
    else                 S.live.volume    = vol;
    positionKnob();
    refreshVolumeControl();
}

function bindVolSlider(el) {
    // Without this a drag on a vertical slider scrolls the page instead of moving
    // the knob — the browser claims the gesture before the first pointermove.
    el.style.touchAction = 'none';
    el.style.userSelect  = 'none';

    el.addEventListener('pointerdown', (e) => {
        el.setPointerCapture(e.pointerId);
        volKnobDragging = true;
        volDragTo(volTravelAt(e.clientX, e.clientY));
    });

    el.addEventListener('pointermove', (e) => {
        if (!volKnobDragging) return;
        volDragTo(volTravelAt(e.clientX, e.clientY));
        if (S.mode === 'bt') return;              // applies on release only
        clearTimeout(volKnobTimeout);
        volKnobTimeout = setTimeout(
            () => wsSend(volumeCommand(travelToVol(volKnobTravel))), 150);
    });

    const end = () => {
        if (!volKnobDragging) return;
        volKnobDragging = false;
        clearTimeout(volKnobTimeout);
        if (!wsSend(volumeCommand(travelToVol(volKnobTravel)))) {
            setWsBadge('down', 'not sent — no connection');
        }
    };
    el.addEventListener('pointerup', end);
    el.addEventListener('pointercancel', end);
}

// position_knob(): the knob travels inside the track, v=100 → right / top.
function positionKnob() {
    const g = S.els.volGeom;
    if (!g || !S.els.volKnob) return;
    const v  = volToTravel(volumeOf());
    const tx = Math.max(g.w - g.knobW, 0);
    const ty = Math.max(g.h - g.knobH, 0);
    S.els.volKnob.style.left = (g.vertical ? g.x + Math.floor((g.w - g.knobW) / 2)
                                           : g.x + Math.floor(tx * v / 100)) + 'px';
    S.els.volKnob.style.top  = (g.vertical ? g.y + Math.floor(ty * (100 - v) / 100)
                                           : g.y + Math.floor((g.h - g.knobH) / 2)) + 'px';

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

// ── Touch hotspots — touch_hotspots_widget.c ────────────────────────────────

const HOTSPOT_COUNT = 8;   // UI_TOUCH_HOTSPOT_COUNT

// control_action_t. The widget skips anything outside PLAY_TOGGLE..OPEN_EQUALIZER,
// so the range doubles as the validity check.
const HOTSPOT_ACTIONS = [
    'Play / stop', 'Previous', 'Next', 'Volume −', 'Volume +',
    'Stop', 'Play / pause', 'Open playlist', 'Open SD browser', 'Open equalizer',
];

// What to send for a hotspot press, or null when the action cannot be driven
// remotely. `source` is the screen's control_source_t, which is what decides the
// mapping — control_action_execute() passes the SCREEN's source, not whatever
// happens to be playing.
//
// On the radio screen each action is expressed as the explicit radio command that
// reproduces media_control_execute(MEDIA_SOURCE_RADIO, …), arithmetic included:
// the semantic plain-text frames (`toggle`, `next`) would act on the active source
// instead, and `volp`/`volm` step by 5 where the hotspot path steps by 2.
//
// On the SD screen the SD-specific frames are exact for next/prev/pause, but
// PLAY_TOGGLE and STOP call sd_player_stop_keep(), which no JSON command exposes.
// There the plain-text `toggle`/`stop` frames ARE the exact path — they route
// through media_control_execute(media_source_current(), …) — but only while SD is
// the active source, which is noted on the page.
function hotspotCommand(action, source) {
    const L = S.live;

    // Volume is the same ±2 arithmetic on every source; only which volume it is
    // differs (BT steps its module's level, see volumeOf).
    if (action === 3 || action === 4) {
        return volumeCommand(clamp(volumeOf(source) + (action === 4 ? 2 : -2), 0, 100), source);
    }

    if (source === 'bt') {
        switch (action) {
            // Both toggles are one and the same on BT (media_control.c: an AVRCP
            // toggle already IS play/pause), and both branch on bt_playing — which
            // is why the state frame carries it. The plain-text `toggle` must NOT be
            // used here: it resolves through media_source_current(), so with the SD
            // player running it would stop SD instead of touching the module, while
            // the panel's own hotspot passes CONTROL_SOURCE_BT and always means BT.
            case 0:
            case 6: return L.bt_playing ? { cmd: 'bt_pause' } : { cmd: 'bt_play' };
            case 1: return { cmd: 'bt_prev' };
            case 2: return { cmd: 'bt_next' };
            case 5: return { cmd: 'bt_pause' };   // MEDIA_ACTION_STOP is a pause here
            default: return null;
        }
    }

    if (source === 'sd') {
        switch (action) {
            case 0: return 'toggle';        // stop_keep / resume — source-aware frame
            case 1: return { cmd: 'sd_prev' };
            case 2: return { cmd: 'sd_next' };
            case 5: return 'stop';
            case 6: return { cmd: 'sd_pause' };
            default: return null;
        }
    }

    const n = S.playlist.length;
    const idx = L.curr_index | 0;
    const playing = L.radio === 'playing';   // BUFFERING is not PLAYING, as in C

    switch (action) {
        case 0:   // PLAY_TOGGLE
        case 6:   // PLAY_PAUSE — "a stream can't pause; same as play/stop"
            return playing ? { cmd: 'stop' } : { cmd: 'play_index', index: idx };
        case 1: return n > 0 ? { cmd: 'play_index', index: (idx - 1 + n) % n } : null;
        case 2: return n > 0 ? { cmd: 'play_index', index: (idx + 1) % n } : null;
        case 5: return { cmd: 'stop' };
        default:
            // OPEN_PLAYLIST / OPEN_SD_BROWSER / OPEN_EQUALIZER navigate the panel's
            // own UI. `set_screen` only reaches radio/home/bt, so there is no frame
            // that can do this — the hotspot stays inert rather than doing something
            // almost-but-not-quite right.
            return null;
    }
}

// Frames whose first byte is not '{' are plain-text commands (see the dispatch
// rule in docs/ws_protocol.md), so a string is sent as-is.
function wsSend(frame) {
    if (!S.ws || S.ws.readyState !== 1) return false;
    S.ws.send(typeof frame === 'string' ? frame : JSON.stringify(frame));
    return true;
}

// The hotspots draw nothing at rest — the wallpaper supplies the artwork and the
// button is a bare touch area over it. They are still built, because their pressed
// highlight is real: holding one shows exactly what a finger sees, which is the
// only way to check a hotspot against the button painted into the wallpaper.
function renderHotspots(frag, p, pre) {
    for (let i = 1; i <= HOTSPOT_COUNT; i++) {
        const key = `${pre}_hotspot_${i}`;
        const w = p[`${key}_w`] | 0, h = p[`${key}_h`] | 0;
        const action = p[`${key}_action`] | 0;
        if (!p[`${key}_enabled`] || w <= 0 || h <= 0 ||
            action < 0 || action >= HOTSPOT_ACTIONS.length) continue;

        const el = box(p[`${key}_x`] | 0, p[`${key}_y`] | 0, w, h, {});
        el.className = 'hotspot';
        // (min(w, h) * clamp(radius, 0, 100)) / 200, integer division included.
        el.style.borderRadius =
            Math.floor((Math.min(w, h) * clamp(p[`${key}_radius`] | 0, 0, 100)) / 200) + 'px';

        const inert = hotspotCommand(action, pre) === null;
        if (inert) {
            el.classList.add('inert');
            el.title = `hotspot ${i}: ${HOTSPOT_ACTIONS[action]} — panel-only, ` +
                       `no WebSocket command can trigger it`;
        } else {
            el.title = `hotspot ${i}: ${HOTSPOT_ACTIONS[action]}`;
            el.addEventListener('click', () => {
                // Re-resolved at click time: the command depends on live state.
                const frame = hotspotCommand(action, pre);
                if (!frame) return;
                if (!wsSend(frame)) setWsBadge('down', 'not sent — no connection');
            });
        }
        frag.appendChild(el);
    }
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

function refreshLive() {
    if      (S.mode === 'radio') refreshRadioLive();
    else if (S.mode === 'sd')    refreshSdLive();
    else                         refreshBtLive();

    if (S.els.clock) setLabelText(S.els.clock, nowString());
    // vol_slider_widget_update() refuses to move the knob while the slider is
    // pressed; without the same guard an arriving broadcast would fight the finger
    // and snap the level back mid-drag.
    if (!volKnobDragging) { positionKnob(); refreshVolumeControl(); }
    refreshListLive();
    refreshEqLive();     // no-op unless the equalizer modal is open
}

// ── Page volume slider ──────────────────────────────────────────────────────
//
// A page control, not part of the rendered screen — the panel's own slider is
// drawn inside the screen from <pre>_volslider_*. This one is the plain 0..100
// volume of whatever source is on show, like the one on the main web UI, so
// <pre>_volslider_vol_max (which only remaps the on-screen slider's travel)
// deliberately does not apply. On BT that is the module's level: dragging this
// while the BT screen is up must not move the radio's volume instead.

const volEl    = document.getElementById('volume');
const volValEl = document.getElementById('vol_value');
let volDragging = false;
let volTimeout  = null;

// vol_slider_widget_update() refuses to move the knob while the slider is pressed;
// the same guard is needed here, or an incoming state broadcast would fight the
// finger mid-drag and snap the slider back.
function refreshVolumeControl() {
    // Until the first state broadcast lands, the device's volume is unknown — so
    // the control stays inert rather than showing a confident 0 % that could be
    // dragged and would overwrite the real level.
    volEl.disabled = !S.gotState;
    if (!S.gotState) { volValEl.textContent = '—'; return; }
    if (volDragging) return;
    volEl.value = clamp(volumeOf(), 0, 100);
    volValEl.textContent = volumeOf() + '%';
}

volEl.addEventListener('input', () => {
    const v = parseInt(volEl.value, 10);
    volValEl.textContent = v + '%';
    // Same 150 ms debounce the main web UI uses: dragging fires 'input' per pixel
    // and every frame would otherwise be a settings write on the device.
    clearTimeout(volTimeout);
    volTimeout = setTimeout(() => {
        if (!wsSend(volumeCommand(v))) setWsBadge('down', 'not sent — no connection');
    }, 150);
});

volEl.addEventListener('pointerdown',   () => { volDragging = true; });
volEl.addEventListener('pointerup',     () => { volDragging = false; });
volEl.addEventListener('pointercancel', () => { volDragging = false; });
volEl.addEventListener('blur',          () => { volDragging = false; });

// ── WebSocket ───────────────────────────────────────────────────────────────

function setWsBadge(cls, text) {
    const b = document.getElementById('ws_badge');
    b.className = 'badge' + (cls ? ' ' + cls : '');
    b.textContent = text;
}

function connectWs() {
    S.ws = new WebSocket(`ws://${location.host}/ws`);

    S.ws.onopen = () => {
        S.wsRetry = 250;
        setWsBadge('live', 'live');
        if (S.mode === 'sd') requestSdList();
    };
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

        if (d.type === 'sd_list') { S.sdList = d; renderListScreen(); return; }
        if (d.type !== 'state') return;

        for (const k of Object.keys(S.live)) {
            if (d[k] !== undefined) S.live[k] = d[k];
        }
        // Anchor for the extrapolated playback position (see docs/ws_protocol.md):
        // every broadcast re-anchors it, so drift cannot accumulate.
        if (d.sd_position_ms !== undefined) S.sdPosAt = performance.now();
        S.gotState = true;
        refreshLive();
    };
}

function requestSdList(dir) {
    wsSend(dir ? { cmd: 'sd_list', dir } : { cmd: 'sd_list' });
}

// ─────────────────────────────────────────────────────────────────────────────
// Zoom / fullscreen
// ─────────────────────────────────────────────────────────────────────────────

// On a phone the layout viewport and the visible one part company — the address
// bar collapses, the keyboard opens, a pinch changes the visual viewport alone.
// visualViewport is the one that says how much screen there actually is.
function viewportSize() {
    const vv = window.visualViewport;
    return {
        w: vv ? vv.width  : window.innerWidth,
        h: vv ? vv.height : window.innerHeight,
    };
}

// Fit may go BELOW 1:1. It used to be clamped there to keep glyphs crisp, which
// silently broke every screen narrower than its panel: a 480 px panel on a 390 px
// phone was pinned to 1:1 and simply hung off the side. A slightly soft panel that
// fits beats a sharp one you cannot see.
const MIN_FIT_SCALE = 0.2;

// Everything between the viewport edge and the panel: the page's own padding plus
// the frame's padding and border. Read from the computed styles rather than
// hard-coded, because the narrow-screen media query changes the page padding — a
// baked-in number would quietly mis-size the fit on exactly the device that has no
// pixels to spare.
function frameChrome() {
    const b = getComputedStyle(document.body);
    const f = getComputedStyle(frameEl);
    const n = (v) => parseFloat(v) || 0;
    return {
        w: n(b.paddingLeft) + n(b.paddingRight)
         + n(f.paddingLeft) + n(f.paddingRight)
         + n(f.borderLeftWidth) + n(f.borderRightWidth),
        h: n(f.paddingTop) + n(f.paddingBottom)
         + n(f.borderTopWidth) + n(f.borderBottomWidth)
         + n(b.paddingBottom),
    };
}

function applyZoom() {
    const mode = document.getElementById('zoom').value;
    const w = S.meta.screen_w, h = S.meta.screen_h;
    const vp = viewportSize();
    let k;

    if (document.fullscreenElement === viewerEl) {
        // On the wall the panel should be as large as it goes, so the zoom picker
        // is ignored and both axes are fitted. Both the volume bar and the list
        // panel share the fullscreen column, so the height is split: the bar and
        // the gaps come off the top, the rest divides between the panels.
        const panels  = listWrapEl.hidden ? 1 : 2;
        const gaps    = 18 * (panels === 2 ? 2 : 1) + 16;
        const usableH = Math.max(vp.h - volbarEl.offsetHeight - gaps, 40);
        k = Math.min(vp.w / w, usableH / (h * panels));
    } else if (mode === 'fit') {
        // Fit the WINDOW, not just its width. Measure the page, never the frame:
        // the frame is inline-block and sized BY the stage, so asking it how wide
        // it is would just echo the last zoom back. offsetTop is scroll-independent.
        // Take the smaller of the two widths: while the page still overflows from a
        // previous (too large) zoom, body.clientWidth can be the wider one.
        const chrome = frameChrome();
        const availW = Math.min(document.body.clientWidth, vp.w) - chrome.w;
        const availH = vp.h - frameEl.offsetTop - volbarEl.offsetHeight - chrome.h;
        k = clamp(Math.min(availW / w, availH / h), MIN_FIT_SCALE, 4);
    } else {
        k = parseFloat(mode);
    }

    S.scale = k;
    for (const [scr, stg] of [[screenEl, stageEl], [listScreen, listStage]]) {
        scr.style.transform = `scale(${k})`;
        stg.style.width  = Math.round(w * k) + 'px';
        stg.style.height = Math.round(h * k) + 'px';
    }
    volbarEl.style.width = frameEl.offsetWidth + 'px';
}

const fullscreenBtn = document.getElementById('fullscreen');

function toggleFullscreen() {
    if (document.fullscreenElement) {
        document.exitFullscreen();
    } else {
        viewerEl.requestFullscreen().catch(err => fail('Fullscreen refused: ' + err.message));
    }
}

// The one button toggles both ways, and it rides inside the fullscreen element,
// so it is still there to get back out — Esc is not the only exit.
fullscreenBtn.addEventListener('click', toggleFullscreen);

let listHiddenBeforeFs = null;

document.addEventListener('fullscreenchange', () => {
    const on = document.fullscreenElement === viewerEl;
    fullscreenBtn.textContent = on ? '⛶ Exit fullscreen' : '⛶ Fullscreen';

    if (on) {
        // The toggle lives in the toolbar, outside the fullscreen element and so
        // not rendered — the list has to come up with the panel, or there would be
        // no way to reveal it from the wall.
        listHiddenBeforeFs = listWrapEl.hidden;
        setListVisible(true);
    } else if (listHiddenBeforeFs !== null) {
        setListVisible(!listHiddenBeforeFs);
        listHiddenBeforeFs = null;
    }
    // Measured on the next frame, not now: leaving fullscreen fires this event
    // before the page is back at its normal size, so measuring here would size the
    // panels against the fullscreen viewport and leave them overflowing.
    if (S.meta) requestAnimationFrame(applyZoom);
});

// ─────────────────────────────────────────────────────────────────────────────
// Mode + list toggle
// ─────────────────────────────────────────────────────────────────────────────

const toggleListBtn = document.getElementById('toggle_list');

// What the button says the user wants. Kept separate from the panel's actual
// visibility so that switching to BT — which has no list at all — hides the panel
// without forgetting the preference for when radio or SD comes back.
let listWanted = true;

function listButtonLabel() {
    const cfg = modeCfg();
    if (!cfg.list) return 'No list screen';
    const what = cfg.list === 'playlist' ? 'playlist' : 'browser';
    return (listWrapEl.hidden ? 'Show ' : 'Hide ') + what + ' screen';
}

function setListVisible(show) {
    const has = !!modeCfg().list;
    listWrapEl.hidden = !has || !show;
    toggleListBtn.disabled = !has;
    toggleListBtn.textContent = listButtonLabel();
    if (S.meta) applyZoom();
}

toggleListBtn.addEventListener('click', () => {
    listWanted = listWrapEl.hidden;
    setListVisible(listWanted);
});

// One control, which is why it lives inside #viewer: it has to keep working in
// fullscreen, and a second copy would need syncing (and would go stale silently —
// re-picking an option a select already shows fires no change event).
const modeEl = document.getElementById('mode');

async function setMode(value) {
    S.mode = value;
    try { localStorage.setItem(MODE_KEY, value); } catch (e) { /* private mode */ }
    listCapEl.textContent = modeCfg().listCaption;
    setListVisible(listWanted);
    // The browser's rows come from the device, one folder at a time.
    if (S.mode === 'sd' && !S.sdList) requestSdList();
    await renderEverything();
}

modeEl.addEventListener('change', (e) => setMode(e.target.value));

// ─────────────────────────────────────────────────────────────────────────────
// Orchestration
// ─────────────────────────────────────────────────────────────────────────────

function fail(msg) {
    const el = document.getElementById('err');
    el.textContent = msg;
    el.style.display = 'block';
}

async function renderEverything() {
    S.els = {};
    const cfg = modeCfg();

    if      (S.mode === 'radio') await renderRadioScreen();
    else if (S.mode === 'sd')    await renderSdScreen();
    else                         await renderBtScreen();

    await applyBackground(screenEl, primaryProfile()[`${cfg.primary}_wallpaper`]);

    if (cfg.list) {
        renderListScreen();
        await applyBackground(listScreen, listProfile()[`${cfg.list}_wallpaper`]);
    }
    applyZoom();
}

// The device's socket table is seven entries wide (HTTPD_MAX_OPEN_SOCKETS) and
// shared with every other client, the Android app's WebSocket included. A browser
// opens up to six parallel connections per host, so firing this page's nine boot
// requests at once fills the table on its own; httpd then LRU-purges the
// longest-idle session, and since state is only broadcast when it changes, that
// is typically somebody's live WebSocket — the app went offline and reconnected
// every time this page was opened. Two at a time stays inside the budget, and it
// keeps the burst from competing with a playing stream for internal RAM, which is
// the same reason max_open_sockets is only seven.
const BOOT_FETCH_CONCURRENCY = 2;

async function getAll(urls, get) {
    const out = new Array(urls.length);
    let next = 0;
    const worker = async () => {
        while (next < urls.length) {
            const i = next++;
            out[i] = await get(urls[i]);
        }
    };
    const workers = Math.min(BOOT_FETCH_CONCURRENCY, urls.length);
    await Promise.all(Array.from({ length: workers }, worker));
    return out;
}

async function loadAll() {
    const get = async (url) => {
        const r = await fetch(url, { cache: 'no-store' });
        if (!r.ok) throw new Error(url + ' → HTTP ' + r.status);
        return r.json();
    };
    const sections = ['radio', 'playlist', 'sd', 'browser', 'bt', 'eq'];
    const [meta, settings, theme, ...profs] = await getAll([
        '/api/ui/profile/meta',
        '/api/settings',
        '/api/theme',
        ...sections.map(s => '/api/ui/profile/' + s),
    ], get);
    S.meta     = meta;
    S.settings = settings;
    S.pal      = theme[theme.current] || theme.dark;
    sections.forEach((s, i) => { S.prof[s] = profs[i]; });

    for (const el of [screenEl, listScreen]) {
        el.style.width  = meta.screen_w + 'px';
        el.style.height = meta.screen_h + 'px';
    }

    // Kept out of the group above on purpose: the station list is only needed by
    // the playlist screen and the station icon, so a playlist that fails to load
    // must not take the screens with it.
    try {
        const pl = await get('/api/playlist');
        S.playlist = Array.isArray(pl) ? pl : [];
    } catch (err) {
        S.playlist = [];
        console.warn('Playlist unavailable:', err.message);
    }
}

async function boot() {
    try {
        await loadAll();
        // Measure the webfont, never the fallback: the baseline correction is only
        // meaningful once the real glyph metrics are in.
        await document.fonts.load('500 16px AtlasMontserrat');
        await document.fonts.ready;
        baselineFix.clear();

        // Restore the source before the first render, and before the socket opens:
        // onopen asks for the SD listing when the mode is already 'sd', so nothing
        // extra has to be kicked off here.
        S.mode = storedMode();
        modeEl.value = S.mode;

        listCapEl.textContent = modeCfg().listCaption;
        setListVisible(listWanted);
        await renderEverything();
        connectWs();
        setInterval(() => { if (S.els.clock) setLabelText(S.els.clock, nowString()); }, 10000);
    } catch (err) {
        fail('Could not load the screens: ' + err.message);
    }
}

document.getElementById('zoom').addEventListener('change', applyZoom);
document.getElementById('show_hotspots').addEventListener('change', (e) => {
    screenEl.classList.toggle('show-hotspots', e.target.checked);
});
window.addEventListener('resize', () => { if (S.meta) applyZoom(); });
window.addEventListener('orientationchange', () => {
    // The new size is not in yet when this fires.
    if (S.meta) requestAnimationFrame(applyZoom);
});
// A phone's address bar collapsing changes the visible height without a resize
// event, so the fit would stay sized against the taller viewport.
if (window.visualViewport) {
    window.visualViewport.addEventListener('resize', () => { if (S.meta) applyZoom(); });
}
document.getElementById('reload').addEventListener('click', async () => {
    try {
        await loadAll();
        baselineFix.clear();
        await renderEverything();
    } catch (err) {
        fail('Reload failed: ' + err.message);
    }
});

window.addEventListener('DOMContentLoaded', boot);
