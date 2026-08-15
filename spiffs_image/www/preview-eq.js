'use strict';

// SCREEN_EQ — mirrors screen_equalizer.c.
//
// Not a source but a screen reached from radio/SD/BT, so it lives in a modal
// instead of the source list. Unlike the other screens it is genuinely two-way:
// the ten gains are in the state broadcast (`eq`) and `set_eq_10` writes them
// back, so dragging a band here does to the device exactly what dragging it on
// the panel does — including the timing, see eqSend().

const EQ_BANDS    = 10;
const EQ_GAIN_MIN = -13;
const EQ_GAIN_MAX = 6;
const EQ_SPAN     = EQ_GAIN_MAX - EQ_GAIN_MIN;

const EQ_FREQ_LABELS = ['31', '62', '125', '250', '500', '1k', '2k', '4k', '8k', '16k'];

// LVGL's default theme pads the slider knob outward by LV_DPX_CALC(dpi, 6),
// which is 5 px at the firmware's CONFIG_LV_DPI_DEF=130. That padding is what
// makes the knob wider than the thin track, and what the screen reserves
// ext_draw room for so it is not clipped at the ends.
const EQ_KNOB_PAD = 5;
const EQ_KNOB_DIM = 0.4;              // EQ_KNOB_DIM_OPA = LV_OPA_40

// update_eq_curve(): 8 samples per segment through a Catmull-Rom spline.
const EQ_CURVE_SEG_SAMPLES = 8;

const eqModal  = document.getElementById('eq_modal');
const eqStage  = document.getElementById('eq_stage');
const eqScreen = document.getElementById('eq_screen');
const eqHeadEl = eqModal.querySelector('.modal-head');
const eqFrameEl = eqStage.parentElement;

const EQ = {
    gains: new Array(EQ_BANDS).fill(0),
    focus: 0,             // s_focus — the band the value label describes
    geom:  null,
    els:   { bands: [] },
    drag:  -1,            // band being dragged, -1 when idle
    knob:  null,          // { url, w, h } once the artwork is decoded
};

// ── Geometry — ui_profile_eq_group_box() + band_x() ─────────────────────────

// Every division here is C integer division: band_x() truncates and so does the
// knob centring, and a rounded-up pixel would put the bands where the panel does
// not have them.
function eqGeom(p) {
    const sw       = Math.max(p.eq_slider_w | 0, 1);
    const sliderH  = Math.max(p.eq_slider_h | 0, 1);
    const gw       = Math.max(p.eq_group_w | 0, sw);
    const freqFh   = lvMetrics(p.eq_freq_font).lh;
    const freqArea = p.eq_freq_hide ? 0 : freqFh + 6;
    const travel   = Math.max(gw - sw, 0);
    const steps    = EQ_BANDS - 1;
    return {
        gx: p.eq_group_x | 0, gy: p.eq_group_y | 0, gw, sw, sliderH, freqArea, freqFh,
        // s_band_step: the average gap, used for the label column and the auto
        // knob width — not for placing the bands.
        step: Math.max(Math.floor((travel + steps / 2) / steps), 1),
        bandX: (i) => Math.floor((i * travel + Math.floor(steps / 2)) / steps),
    };
}

// ── Curve — a straight port of update_eq_curve() ────────────────────────────

function eqCurvePoints(cw, ch) {
    const gy = EQ.gains.map(g => (EQ_GAIN_MAX - g) / EQ_SPAN * ch);   // gain=max → top
    const pts = [];

    for (let seg = 0; seg < EQ_BANDS - 1; seg++) {
        const p0 = gy[seg > 0 ? seg - 1 : 0];
        const p1 = gy[seg];
        const p2 = gy[seg + 1];
        const p3 = gy[seg < EQ_BANDS - 2 ? seg + 2 : EQ_BANDS - 1];
        for (let s = 0; s < EQ_CURVE_SEG_SAMPLES; s++) {
            const t = s / EQ_CURVE_SEG_SAMPLES, t2 = t * t, t3 = t2 * t;
            let y = 0.5 * ((2 * p1) + (-p0 + p2) * t
                         + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2
                         + (-p0 + 3 * p1 - 3 * p2 + p3) * t3);
            const x = (seg + t) / (EQ_BANDS - 1) * cw;
            y = clamp(y, 0, ch);
            pts.push(`${Math.floor(x + 0.5)},${Math.floor(y + 0.5)}`);
        }
    }
    pts.push(`${cw},${Math.floor(gy[EQ_BANDS - 1] + 0.5)}`);
    return pts.join(' ');
}

function updateEqCurve() {
    const c = EQ.els.curve;
    if (!c) return;
    c.line.setAttribute('points', eqCurvePoints(c.w, c.h));
}

// ── Bands ───────────────────────────────────────────────────────────────────

// lv_bar: the indicator length truncates, and it grows from the bottom edge.
function eqIndicH(gain, sliderH) {
    return Math.floor(sliderH * (gain - EQ_GAIN_MIN) / EQ_SPAN);
}

function updateEqBand(i) {
    const b = EQ.els.bands[i];
    if (!b) return;
    const g = EQ.geom, gain = EQ.gains[i];
    const indicH = eqIndicH(gain, g.sliderH);
    const top    = g.gy + g.sliderH - indicH;

    if (b.indic) {
        b.indic.style.top    = top + 'px';
        b.indic.style.height = indicH + 'px';
    }

    if (b.knobImg) {
        // position_eq_knob(): the artwork travels inside the slider length, so it
        // never overhangs the ends the way the native knob does.
        const travelY = Math.max(g.sliderH - EQ.knob.h, 0);
        b.knobImg.style.top  = (g.gy + Math.floor(travelY * (EQ_GAIN_MAX - gain) / EQ_SPAN)) + 'px';
    } else if (b.knob) {
        // position_knob(): centred on the indicator's top edge, then grown by the
        // knob padding on every side.
        b.knob.style.top = (top - (g.sw >> 1) - EQ_KNOB_PAD) + 'px';
    }
}

// update_focus_visuals(): the indicator and the frequency label take the accent
// on the active band; artwork knobs dim instead of taking an outline, because the
// outline would paint over the picture.
function updateEqFocus() {
    const th = S.pal;
    for (let i = 0; i < EQ_BANDS; i++) {
        const b = EQ.els.bands[i];
        if (!b) continue;
        const on = i === EQ.focus;
        if (b.indic)   b.indic.style.background = on ? th.accent : th.text_muted;
        if (b.freq)    b.freq.style.color       = on ? th.accent : th.text_muted;
        if (b.knobImg) b.knobImg.style.opacity  = on ? '1' : String(EQ_KNOB_DIM);
        if (b.knob)    b.knob.style.border      = on ? `2px solid ${th.accent}` : '';
    }
    updateEqInfo();
}

// update_info_label(): "%s Hz: %+d dB" — the sign is always printed.
function updateEqInfo() {
    const el = EQ.els.info;
    if (!el) return;
    const g = EQ.gains[EQ.focus];
    el._span.textContent =
        `${EQ_FREQ_LABELS[EQ.focus]} Hz: ${g >= 0 ? '+' : ''}${g} dB`;
}

// ── Knob artwork — build_eq_knobs() ─────────────────────────────────────────

// One .bin decoded and scaled once, then shown per band. An "asset<N>" reference
// is an internet slot resolved by ui_asset on the device; the browser has no way
// to read those, so those fall back to the plain knob — the same choice the
// volume slider's preview makes.
async function loadEqKnob(p, geom) {
    const ref = String(p.eq_knob_image || '').trim();
    if (!ref || /^asset\d$/.test(ref)) return null;
    try {
        const rel = ref.startsWith('/sdcard/') ? ref.slice('/sdcard'.length) : ref;
        const f = await fetch('/api/sd/file?path=' + encodeURIComponent(rel), { cache: 'no-store' });
        if (!f.ok) throw new Error('HTTP ' + f.status);
        const dec = window.LvBin.decodeToCanvas(await f.arrayBuffer());

        let kw = (p.eq_knob_w | 0) > 0 ? (p.eq_knob_w | 0) : geom.step;
        let kh = dec.w > 0 ? Math.floor(dec.h * kw / dec.w) : kw;
        if (kh > geom.sliderH) {                       // very tall art — cap, keep aspect
            kh = geom.sliderH;
            kw = dec.h > 0 ? Math.floor(dec.w * kh / dec.h) : kw;
        }
        return { art: dec.canvas, w: Math.max(kw, 1), h: Math.max(kh, 1) };
    } catch {
        // No artwork on the card is the normal case — the themed knob stays.
        return null;
    }
}

// ── Screen ──────────────────────────────────────────────────────────────────

async function renderEqScreen() {
    const p  = S.prof.eq;
    const th = S.pal;
    if (!p) return;

    EQ.gains = (Array.isArray(S.live.eq) && S.live.eq.length === EQ_BANDS)
        ? S.live.eq.slice() : new Array(EQ_BANDS).fill(0);
    EQ.focus = 0;                      // eq_create() starts on the first band
    EQ.els   = { bands: [] };
    const g  = EQ.geom = eqGeom(p);
    EQ.knob  = await loadEqKnob(p, g);

    const frag = document.createDocumentFragment();

    // Value label — the one label on this screen with no scrim behind it, and
    // aligned TOP_LEFT rather than centre-anchored, so x/y is its corner.
    EQ.els.info = makeLabel({
        x: p.eq_info_x | 0, y: p.eq_info_y | 0, fontId: p.eq_info_font,
        text: '', color: th.text_primary, align: 'left', plate: 0,
    });
    frag.appendChild(EQ.els.info);

    // Response curve — drawn only when the box has room, exactly as eq_create()
    // decides. The box arrives already resolved (dump_eq sends the effective one).
    const cw = p.eq_curve_w | 0, ch = p.eq_curve_h | 0;
    if (ch >= 12 && cw >= 40) {
        const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
        svg.setAttribute('width', cw);
        svg.setAttribute('height', ch);
        svg.style.position = 'absolute';
        svg.style.left = (p.eq_curve_x | 0) + 'px';
        svg.style.top  = (p.eq_curve_y | 0) + 'px';
        svg.style.overflow = 'visible';
        const line = document.createElementNS('http://www.w3.org/2000/svg', 'polyline');
        line.setAttribute('fill', 'none');
        line.setAttribute('stroke', th.accent);
        line.setAttribute('stroke-width', '2');
        line.setAttribute('stroke-linecap', 'round');
        line.setAttribute('stroke-linejoin', 'round');
        svg.appendChild(line);
        frag.appendChild(svg);
        EQ.els.curve = { line, w: cw, h: ch };
    }

    const knobOnly = !!p.eq_knob_only;

    for (let i = 0; i < EQ_BANDS; i++) {
        const x = g.gx + g.bandX(i);
        const b = {};

        // Track and fill keep the theme's fully rounded ends; knob-only hides both
        // so nothing but the artwork shows over the wallpaper.
        if (!knobOnly) {
            frag.appendChild(box(x, g.gy, g.sw, g.sliderH, {
                background: th.text_muted, borderRadius: (g.sw / 2) + 'px',
            }));
            b.indic = box(x, g.gy, g.sw, 0, {
                background: th.accent, borderRadius: (g.sw / 2) + 'px',
            });
            frag.appendChild(b.indic);
        }

        if (EQ.knob) {
            // Sibling above the slider, not a child — it is not clickable on the
            // device either; the band underneath owns the drag.
            b.knobImg = box(x + Math.trunc((g.sw - EQ.knob.w) / 2), g.gy,
                            EQ.knob.w, EQ.knob.h, { pointerEvents: 'none' });
            paintArt(b.knobImg, EQ.knob.art);
            frag.appendChild(b.knobImg);
        } else {
            const size = g.sw + 2 * EQ_KNOB_PAD;
            b.knob = box(x - EQ_KNOB_PAD, g.gy, size, size, {
                background: th.accent, borderRadius: '50%',
                boxSizing: 'border-box', pointerEvents: 'none',
            });
            frag.appendChild(b.knob);
        }

        // Frequency label: one step wide, centred on its band, below the slider.
        if (!p.eq_freq_hide) {
            const lbl = alignedText({
                text: EQ_FREQ_LABELS[i], fontId: p.eq_freq_font, color: th.text_muted,
                left: x - Math.trunc((g.step - g.sw) / 2), top: g.gy + g.sliderH + 2,
            });
            lbl.style.width     = g.step + 'px';
            lbl.style.textAlign = 'center';
            frag.appendChild(lbl);
            b.freq = lbl;
        }

        // The drag target is the track grown by the knob padding — the width a
        // finger actually gets on the panel, where the knob area is what LVGL
        // hit-tests.
        const hit = box(x - EQ_KNOB_PAD, g.gy, g.sw + 2 * EQ_KNOB_PAD, g.sliderH, {});
        hit.className = 'eq-band';
        hit.title = `${EQ_FREQ_LABELS[i]} Hz`;
        bindEqBand(hit, i);
        frag.appendChild(hit);

        EQ.els.bands.push(b);
    }

    // Hint — BOTTOM_MID with the profile's offsets. The device swaps this text for
    // "press=next  long=back" on encoder-only panels (settings_show_slider), which
    // is compile-time only and so cannot be told apart from here.
    if (!p.eq_hint_hide) {
        const lh = lvMetrics(p.eq_hint_font).lh;
        frag.appendChild(makeLabel({
            x: Math.floor(S.meta.screen_w / 2) + (p.eq_hint_x | 0),
            y: S.meta.screen_h + (p.eq_hint_y | 0) - lh,
            fontId: p.eq_hint_font, text: 'swipe = back', color: th.text_muted,
        }));
    }

    eqScreen.replaceChildren(frag);
    for (let i = 0; i < EQ_BANDS; i++) updateEqBand(i);
    updateEqCurve();
    updateEqFocus();

    await applyBackground(eqScreen, p.eq_wallpaper);
    applyEqZoom();
}

// ── Dragging a band ─────────────────────────────────────────────────────────

// settings_set_eq_10() writes flash, so the device commits on release and not on
// every frame of the drag. Same here — the moving band is local until let go.
function eqSend() {
    if (!wsSend({ cmd: 'set_eq_10', bands: EQ.gains.slice() })) {
        setWsBadge('down', 'not sent — no connection');
    }
}

// Pointer y → gain. The panel maps a touch to the value the same way, give or
// take LVGL's own rounding, which is not worth mirroring for a control.
function eqGainAt(clientY) {
    const g = EQ.geom;
    const rect = eqStage.getBoundingClientRect();
    const localY = (clientY - rect.top) / eqScale - g.gy;
    const frac = clamp((g.sliderH - localY) / g.sliderH, 0, 1);
    return Math.round(EQ_GAIN_MIN + frac * EQ_SPAN);
}

function eqSetGain(i, gain) {
    const v = clamp(gain, EQ_GAIN_MIN, EQ_GAIN_MAX);
    if (EQ.gains[i] === v && EQ.focus === i) return;
    EQ.gains[i] = v;
    if (EQ.focus !== i) { EQ.focus = i; updateEqFocus(); }
    updateEqBand(i);
    updateEqCurve();
    updateEqInfo();
}

function bindEqBand(el, i) {
    el.addEventListener('pointerdown', (e) => {
        el.setPointerCapture(e.pointerId);
        EQ.drag = i;
        eqSetGain(i, eqGainAt(e.clientY));
    });
    el.addEventListener('pointermove', (e) => {
        if (EQ.drag === i) eqSetGain(i, eqGainAt(e.clientY));
    });
    const end = () => {
        if (EQ.drag !== i) return;
        EQ.drag = -1;
        eqSend();
    };
    el.addEventListener('pointerup', end);
    el.addEventListener('pointercancel', end);
}

// ── Live ────────────────────────────────────────────────────────────────────

// eq_on_event(): an external change (web UI, Android, another browser) refreshes
// the bands. Skipped mid-drag — the finger wins until it is lifted.
function refreshEqLive() {
    if (!eqModal.open || EQ.drag >= 0 || !EQ.geom) return;
    const eq = S.live.eq;
    if (!Array.isArray(eq) || eq.length !== EQ_BANDS) return;
    if (eq.every((v, i) => v === EQ.gains[i])) return;

    EQ.gains = eq.slice();
    for (let i = 0; i < EQ_BANDS; i++) updateEqBand(i);
    updateEqCurve();
    updateEqInfo();
}

// ── Zoom / open / close ─────────────────────────────────────────────────────

let eqScale = 1;

function applyEqZoom() {
    if (!S.meta) return;
    const w = S.meta.screen_w, h = S.meta.screen_h;
    const n = (v) => parseFloat(v) || 0;
    const d = getComputedStyle(eqModal), f = getComputedStyle(eqFrameEl);
    const chromeW = n(d.paddingLeft) + n(d.paddingRight)
                  + n(d.borderLeftWidth) + n(d.borderRightWidth)
                  + n(f.paddingLeft) + n(f.paddingRight)
                  + n(f.borderLeftWidth) + n(f.borderRightWidth);
    const chromeH = n(d.paddingTop) + n(d.paddingBottom)
                  + n(d.borderTopWidth) + n(d.borderBottomWidth)
                  + n(f.paddingTop) + n(f.paddingBottom)
                  + n(f.borderTopWidth) + n(f.borderBottomWidth)
                  + eqHeadEl.offsetHeight + 8;

    // The dialog is sized BY the panel, so it is measured against the window (the
    // same 96 % the CSS caps it at), never against itself.
    const vp = viewportSize();
    eqScale = clamp(Math.min((vp.w * 0.96 - chromeW) / w, (vp.h * 0.96 - chromeH) / h),
                    MIN_FIT_SCALE, 4);

    eqScreen.style.transform = `scale(${eqScale})`;
    eqStage.style.width  = Math.round(w * eqScale) + 'px';
    eqStage.style.height = Math.round(h * eqScale) + 'px';
}

document.getElementById('eq_open').addEventListener('click', async () => {
    if (!S.meta) return;                      // still booting
    eqScreen.style.width  = S.meta.screen_w + 'px';
    eqScreen.style.height = S.meta.screen_h + 'px';
    eqModal.showModal();
    // Rendered after opening so the profile is re-read every time (a layout edit
    // in another tab shows up on the next open) and the fit is measured against
    // a dialog that is already laid out.
    try {
        await renderEqScreen();
    } catch (err) {
        fail('Could not draw the equalizer: ' + err.message);
    }
});

document.getElementById('eq_close').addEventListener('click', () => eqModal.close());
eqModal.addEventListener('close', () => { EQ.drag = -1; });

window.addEventListener('resize', () => { if (eqModal.open) applyEqZoom(); });
