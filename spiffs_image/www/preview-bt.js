'use strict';

// SCREEN_BT — mirrors screen_bt.c.
//
// The simplest of the three: no wheels, no VU, no list to pick from — the phone
// owns the queue and the panel only reports what the module tells it. Children are
// appended in bt_create()'s order, so anything that overlaps stacks as it does on
// the panel.

// bt_state_t. The firmware paints all three with the same colour (status_ok or the
// bt_status_color override) and leaves the label untouched for anything else, so an
// unknown state shows as an empty (hidden) label here.
const BT_STATE_TEXT = { 0: 'Connected', 1: 'Not connected', 2: 'Discoverable' };

// LV_SYMBOL_BLUETOOTH is a FontAwesome codepoint from LVGL's built-in symbol font,
// and the Montserrat subset this page ships has no such glyph — so the mark is
// drawn as a path instead, at the icon font's line height. It is the shape the
// panel shows; only the source of the pixels differs.
function btIconMark(size, color) {
    const el = document.createElement('div');
    el.style.position = 'absolute';
    el.style.width    = size + 'px';
    el.style.height   = size + 'px';
    el.style.color    = color;
    el.innerHTML =
        `<svg viewBox="0 0 24 24" width="${size}" height="${size}" fill="none" ` +
        `stroke="currentColor" stroke-width="2" stroke-linecap="round" ` +
        `stroke-linejoin="round"><path d="M7 7 L17 17 L12 22 L12 2 L17 7 L7 17"/></svg>`;
    return el;
}

// ── Screen ──────────────────────────────────────────────────────────────────

async function renderBtScreen() {
    const p    = S.prof.bt;
    const th   = S.pal;
    const opa  = clamp(p.bt_label_bg_opa ?? 50, 0, 100);
    const frag = document.createDocumentFragment();

    renderSharedStubs(frag, p, 'bt');      // the mode indicator, as bt_create() starts
    renderClockWidget(frag, p, 'bt');

    if (p.bt_show_circle) {
        const cw = p.bt_circle_w | 0, ch = p.bt_circle_h | 0;
        const cx = p.bt_circle_x | 0, cy = p.bt_circle_y | 0;
        // LV_RADIUS_CIRCLE on a non-square box is a pill, not an ellipse: LVGL
        // caps the radius at half the shorter side.
        frag.appendChild(box(cx, cy, cw, ch, {
            background: th.bt_brand, borderRadius: Math.floor(Math.min(cw, ch) / 2) + 'px',
        }));

        // lv_obj_center() inside the circle, integer division included.
        const size = lvMetrics(p.bt_icon_font).lh;
        const icon = btIconMark(size, th.text_primary);
        icon.style.left = (cx + Math.floor((cw - size) / 2)) + 'px';
        icon.style.top  = (cy + Math.floor((ch - size) / 2)) + 'px';
        frag.appendChild(icon);
    }

    // Fixed brand line — the one label on this screen whose text never changes.
    frag.appendChild(makeLabel({
        x: p.bt_brand_x | 0, y: p.bt_brand_y | 0, fontId: p.bt_brand_font,
        text: 'Bluetooth Audio', plate: opa,
        color: col(p.bt_brand_color, th.bt_brand),
    }));

    S.els.btStatus = makeLabel({
        x: p.bt_status_x | 0, y: p.bt_status_y | 0, fontId: p.bt_status_font,
        text: '', plate: opa, color: col(p.bt_status_color, th.status_ok),
    });
    frag.appendChild(S.els.btStatus);

    S.els.btVolume = makeLabel({
        x: p.bt_vol_x | 0, y: p.bt_vol_y | 0, fontId: p.bt_vol_label_font,
        text: '', plate: opa, color: col(p.bt_vol_color, th.text_muted),
    });
    frag.appendChild(S.els.btVolume);

    // Title and artist are fixed-width boxes centred on the box centre (x + w / 2),
    // the same shape sd_title and radio_np use.
    const titleW = Math.max(p.bt_title_w | 0, 8);
    S.els.btTitle = makeLabel({
        x: (p.bt_title_x | 0) + titleW / 2, y: p.bt_title_y | 0,
        fontId: p.bt_title_font, text: '', boxW: titleW, plate: opa,
        color: col(p.bt_title_color, th.text_primary),
    });
    frag.appendChild(S.els.btTitle);

    const artistW = Math.max(p.bt_artist_w | 0, 8);
    S.els.btArtist = makeLabel({
        x: (p.bt_artist_x | 0) + artistW / 2, y: p.bt_artist_y | 0,
        fontId: p.bt_artist_font, text: '', boxW: artistW, plate: opa,
        color: col(p.bt_artist_color, th.text_secondary),
    });
    frag.appendChild(S.els.btArtist);

    S.els.btTime = makeLabel({
        x: p.bt_time_x | 0, y: p.bt_time_y | 0, fontId: p.bt_time_font,
        text: '0:00 / 0:00', plate: opa,
        color: col(p.bt_time_color, th.text_secondary),
    });
    frag.appendChild(S.els.btTime);

    // Created before the slider, exactly as bt_create() orders it — a hotspot over
    // the slider must not shadow it.
    renderHotspots(frag, p, 'bt');

    screenEl.replaceChildren(frag);

    // As on radio and SD, the tap-to-show controls overlay is left out: it only
    // exists once the screen is touched and hides itself again after 1.5 s.

    await renderVolSlider(screenEl, p, 'bt');
    refreshLive();
}

// ── Live ────────────────────────────────────────────────────────────────────

// refresh_from_state(): the position comes from the module's own reports (no
// extrapolation between them — the panel does not interpolate either), and the
// title/artist labels hide themselves while empty so no bare plate shows.
function refreshBtLive() {
    const L = S.live;

    setLabelText(S.els.btStatus, BT_STATE_TEXT[L.bt_state | 0] || '');
    setLabelText(S.els.btVolume, `VOL: ${L.bt_volume | 0}%`);
    setLabelText(S.els.btTitle,  L.bt_title || '');
    setLabelText(S.els.btArtist, L.bt_artist || '');
    setLabelText(S.els.btTime,
        `${fmtMmss((L.bt_position_s | 0) * 1000)} / ${fmtMmss(L.bt_duration_ms | 0)}`);
}
