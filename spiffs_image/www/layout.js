// ════════════════════════════════════════════════════════════════════════════
//   AtlasCube — Layout Editor
//
//   All coordinates of editable elements are absolute relative to the LCD
//   screen (top-left origin). Each "free element" with w/h fields is movable
//   AND resizable (4 corner handles). Elements with only x/y (e.g. labels)
//   are positionable only — their size is determined by font metrics.
//
//   Sections (tabs): clock, bt — each has its own field schema, renderer,
//   and JSON state. Selecting a tab switches what the form edits and what
//   the SVG draws.
// ════════════════════════════════════════════════════════════════════════════

const SVG_NS = 'http://www.w3.org/2000/svg';

// ── Field schemas — order is purely UI grouping; doesn't affect backend ────

const CLOCK_FIELDS = [
    { key: 'clock_panel_x', label: 'Panel X',  type: 'number' },
    { key: 'clock_panel_y', label: 'Panel Y',  type: 'number' },
    { key: 'clock_panel_w', label: 'Panel W',  type: 'number' },
    { key: 'clock_panel_h', label: 'Panel H',  type: 'number' },

    { key: 'clock_show_time', label: 'Show time',     type: 'bool' },
    { key: 'clock_time_x',    label: 'Time X',        type: 'number' },
    { key: 'clock_time_y',    label: 'Time Y',        type: 'number' },
    { key: 'clock_time_font', label: 'Time font',     type: 'font'   },

    { key: 'clock_show_date', label: 'Show date',     type: 'bool' },
    { key: 'clock_date_x',    label: 'Date X',        type: 'number' },
    { key: 'clock_date_y',    label: 'Date Y',        type: 'number' },
    { key: 'clock_date_font', label: 'Date font',     type: 'font'   },

    { key: 'clock_show_strip', label: 'Show strip',   type: 'bool' },
    { key: 'clock_strip_x',    label: 'Strip X',      type: 'number' },
    { key: 'clock_strip_y',    label: 'Strip Y',      type: 'number' },
    { key: 'clock_strip_w',    label: 'Strip W',      type: 'number' },
    { key: 'clock_strip_h',    label: 'Strip H',      type: 'number' },
    { key: 'clock_strip_label_w',         label: 'Strip label W',  type: 'number' },
    { key: 'clock_strip_station_y',       label: 'Station Y',      type: 'number' },
    { key: 'clock_strip_title_y',         label: 'Title Y',        type: 'number' },
    { key: 'clock_strip_station_font',    label: 'Station font',   type: 'font'   },
    { key: 'clock_strip_title_font',      label: 'Title font',     type: 'font'   },

    { key: 'clock_show_mode_indicator',  label: 'Show mode indic.',  type: 'bool' },
    { key: 'clock_mode_indic_x',         label: 'Mode indic. X',     type: 'number' },
    { key: 'clock_mode_indic_y',         label: 'Mode indic. Y',     type: 'number' },
    { key: 'clock_show_event_indicator', label: 'Show event indic.', type: 'bool' },
    { key: 'clock_event_indic_x',        label: 'Event indic. X',    type: 'number' },
    { key: 'clock_event_indic_y',        label: 'Event indic. Y',    type: 'number' },
];

const BT_FIELDS = [
    { key: 'bt_show_circle', label: 'Show circle',     type: 'bool' },
    { key: 'bt_circle_x',    label: 'Circle X',        type: 'number' },
    { key: 'bt_circle_y',    label: 'Circle Y',        type: 'number' },
    { key: 'bt_circle_w',    label: 'Circle W',        type: 'number' },
    { key: 'bt_circle_h',    label: 'Circle H',        type: 'number' },
    { key: 'bt_icon_font',   label: 'Icon font',       type: 'font'   },

    { key: 'bt_brand_x',     label: 'Brand X',         type: 'number' },
    { key: 'bt_brand_y',     label: 'Brand Y',         type: 'number' },
    { key: 'bt_brand_font',  label: 'Brand font',      type: 'font'   },

    { key: 'bt_status_x',    label: 'Status X',        type: 'number' },
    { key: 'bt_status_y',    label: 'Status Y',        type: 'number' },
    { key: 'bt_status_font', label: 'Status font',     type: 'font'   },

    { key: 'bt_slider_x',    label: 'Slider X',        type: 'number' },
    { key: 'bt_slider_y',    label: 'Slider Y',        type: 'number' },
    { key: 'bt_slider_w',    label: 'Slider W',        type: 'number' },
    { key: 'bt_slider_h',    label: 'Slider H',        type: 'number' },

    { key: 'bt_vol_label_x',     label: 'Vol label X',    type: 'number' },
    { key: 'bt_vol_label_y',     label: 'Vol label Y',    type: 'number' },
    { key: 'bt_vol_label_font',  label: 'Vol label font', type: 'font'   },

    { key: 'bt_show_mode_indicator', label: 'Show mode indic.', type: 'bool' },
    { key: 'bt_mode_indic_x',        label: 'Mode indic. X',    type: 'number' },
    { key: 'bt_mode_indic_y',        label: 'Mode indic. Y',    type: 'number' },
    { key: 'bt_show_clock',          label: 'Show clock',       type: 'bool' },
    { key: 'bt_clock_widget_x',      label: 'Clock X',          type: 'number' },
    { key: 'bt_clock_widget_y',      label: 'Clock Y',          type: 'number' },
];

const RADIO_FIELDS = [
    { key: 'radio_show_np',           label: 'Show now-playing', type: 'bool' },
    { key: 'radio_np_x',              label: 'NP X',             type: 'number' },
    { key: 'radio_np_y',              label: 'NP Y',             type: 'number' },

    { key: 'radio_state_x',           label: 'State X',          type: 'number' },
    { key: 'radio_state_y',           label: 'State Y',          type: 'number' },
    { key: 'radio_state_font',        label: 'State font',       type: 'font'   },

    { key: 'radio_audio_info_x',      label: 'Audio info X',     type: 'number' },
    { key: 'radio_audio_info_y',      label: 'Audio info Y',     type: 'number' },
    { key: 'radio_audio_info_font',   label: 'Audio info font',  type: 'font'   },

    { key: 'radio_slider_x',          label: 'Slider X',         type: 'number' },
    { key: 'radio_slider_y',          label: 'Slider Y',         type: 'number' },
    { key: 'radio_slider_w',          label: 'Slider W',         type: 'number' },
    { key: 'radio_slider_h',          label: 'Slider H',         type: 'number' },

    { key: 'radio_vol_label_x',       label: 'Vol label X',      type: 'number' },
    { key: 'radio_vol_label_y',       label: 'Vol label Y',      type: 'number' },
    { key: 'radio_vol_label_font',    label: 'Vol label font',   type: 'font'   },

    { key: 'radio_show_mode_indicator', label: 'Show mode indic.', type: 'bool' },
    { key: 'radio_mode_indic_x',        label: 'Mode indic. X',    type: 'number' },
    { key: 'radio_mode_indic_y',        label: 'Mode indic. Y',    type: 'number' },
    { key: 'radio_show_clock',          label: 'Show clock',       type: 'bool' },
    { key: 'radio_clock_widget_x',      label: 'Clock X',          type: 'number' },
    { key: 'radio_clock_widget_y',      label: 'Clock Y',          type: 'number' },
];

// ── Sections registry ──────────────────────────────────────────────────────
// Each entry: { title, fields, renderer (active section's renderSvg) }

const SECTIONS = {
    clock: { title: 'Clock',     fields: CLOCK_FIELDS, renderer: renderClock },
    bt:    { title: 'Bluetooth', fields: BT_FIELDS,    renderer: renderBt    },
    radio: { title: 'Radio',     fields: RADIO_FIELDS, renderer: renderRadio },
};

const state = {
    meta:   { screen_w: 320, screen_h: 240, fonts: [] },
    active: 'clock',
    clock:  {},
    bt:     {},
    radio:  {},
};

// ── Bootstrap ───────────────────────────────────────────────────────────────

window.addEventListener('DOMContentLoaded', async () => {
    try {
        const meta = await fetch('/api/ui/profile/meta').then(r => r.json());
        state.meta = meta;
        document.getElementById('screen_dim').textContent =
            `${meta.screen_w} × ${meta.screen_h}`;

        // Pre-fetch every section so switching tabs is instant
        for (const name of Object.keys(SECTIONS)) {
            state[name] = await fetch(`/api/ui/profile/${name}`).then(r => r.json());
        }

        selectSection('clock');
    } catch (err) {
        setStatus('Failed to load profile: ' + err.message, true);
    }
});

function selectSection(name) {
    if (!SECTIONS[name]) return;
    state.active = name;

    document.getElementById('form_section_title').textContent = SECTIONS[name].title;
    for (const tab of document.querySelectorAll('.section-tab')) {
        tab.classList.toggle('active', tab.dataset.section === name);
    }

    buildForm();
    renderSvg();
}

// ── Form ────────────────────────────────────────────────────────────────────

function buildForm() {
    const root  = document.getElementById('form_section');
    const data  = state[state.active];
    const fields = SECTIONS[state.active].fields;
    [...root.querySelectorAll('.form-row')].forEach(n => n.remove());

    for (const f of fields) {
        const row = document.createElement('div');
        row.className = 'form-row';

        const lab = document.createElement('label');
        lab.textContent = f.label;
        lab.htmlFor = 'fld_' + f.key;
        row.appendChild(lab);

        let input;
        if (f.type === 'number') {
            input = document.createElement('input');
            input.type = 'number';
            input.value = data[f.key] ?? 0;
            input.addEventListener('input', () => {
                data[f.key] = parseInt(input.value, 10) | 0;
                renderSvg();
            });
        } else if (f.type === 'bool') {
            input = document.createElement('input');
            input.type = 'checkbox';
            input.checked = !!data[f.key];
            input.addEventListener('change', () => {
                data[f.key] = input.checked;
                renderSvg();
            });
        } else if (f.type === 'font') {
            input = document.createElement('select');
            for (const id of state.meta.fonts) {
                const o = document.createElement('option');
                o.value = id;
                o.textContent = id;
                input.appendChild(o);
            }
            input.value = data[f.key] ?? '';
            input.addEventListener('change', () => {
                data[f.key] = input.value;
                renderSvg();
            });
        }
        input.id = 'fld_' + f.key;
        row.appendChild(input);
        root.appendChild(row);
    }
}

function setFormValue(key, val) {
    const el = document.getElementById('fld_' + key);
    if (!el) return;
    if (el.type === 'checkbox') el.checked = !!val;
    else                        el.value   = val;
}

// ── SVG render — dispatch to per-section renderer ───────────────────────────

const SCALE = 2;
const HANDLE_SIZE = 4;

function renderSvg() {
    const svg = document.getElementById('lcd');
    const W = state.meta.screen_w, H = state.meta.screen_h;
    svg.setAttribute('viewBox', `0 0 ${W} ${H}`);
    svg.setAttribute('width',  W * SCALE);
    svg.setAttribute('height', H * SCALE);
    svg.innerHTML = '';

    rect(svg, { x: 0, y: 0, width: W, height: H, class: 'lcd-bg' });
    SECTIONS[state.active].renderer(svg);
}

// ── CLOCK renderer ──────────────────────────────────────────────────────────

function renderClock(svg) {
    const c = state.clock;
    const W = state.meta.screen_w;

    drawFreeElement(svg, {
        x: c.clock_panel_x, y: c.clock_panel_y,
        w: c.clock_panel_w, h: c.clock_panel_h,
        label: 'panel', cls: 'panel',
        fields: { x: 'clock_panel_x', y: 'clock_panel_y',
                  w: 'clock_panel_w', h: 'clock_panel_h' },
    });

    if (c.clock_show_time) {
        const fh = fontHeight(c.clock_time_font);
        const tw = Math.round(fh * 0.55) * 5;     // "00:00"
        drawFreeElement(svg, {
            x: c.clock_time_x, y: c.clock_time_y, w: tw, h: fh,
            label: 'time', cls: 'label-rect',
            fields: { x: 'clock_time_x', y: 'clock_time_y' },
            text: '88:88', textSize: fh,
        });
    }
    if (c.clock_show_date) {
        const fh = fontHeight(c.clock_date_font);
        const tw = Math.round(fh * 0.55) * 16;    // "Mon  YYYY-MM-DD"
        drawFreeElement(svg, {
            x: c.clock_date_x, y: c.clock_date_y, w: tw, h: fh,
            label: 'date', cls: 'label-rect',
            fields: { x: 'clock_date_x', y: 'clock_date_y' },
            text: 'Mon  2026-05-01', textSize: fh,
        });
    }

    if (c.clock_show_mode_indicator) {
        drawFreeElement(svg, {
            x: c.clock_mode_indic_x, y: c.clock_mode_indic_y, w: 16, h: 16,
            label: 'mode', cls: 'label-rect',
            fields: { x: 'clock_mode_indic_x', y: 'clock_mode_indic_y' },
        });
    }
    if (c.clock_show_event_indicator) {
        drawFreeElement(svg, {
            x: c.clock_event_indic_x, y: c.clock_event_indic_y, w: 16, h: 16,
            label: 'evt', cls: 'label-rect',
            fields: { x: 'clock_event_indic_x', y: 'clock_event_indic_y' },
        });
    }

    if (c.clock_show_strip) {
        const sx = c.clock_strip_x, sy = c.clock_strip_y;
        const sw = c.clock_strip_w, sh = c.clock_strip_h;
        drawFreeElement(svg, {
            x: sx, y: sy, w: sw, h: sh,
            label: 'strip', cls: 'panel',
            fields: { x: 'clock_strip_x', y: 'clock_strip_y',
                      w: 'clock_strip_w', h: 'clock_strip_h' },
        });

        const labW = clamp(c.clock_strip_label_w, 8, sw);
        drawStripLabel(svg, sx, sy, c.clock_strip_station_y, labW, sw,
                       'station', 'clock_strip_station_y');
        drawStripLabel(svg, sx, sy, c.clock_strip_title_y, labW, sw,
                       'title', 'clock_strip_title_y');
    }
}

function drawStripLabel(svg, sx, sy, yWithinStrip, labelW, stripW, name, fieldKey) {
    const x = sx + (stripW - labelW) / 2;
    const y = sy + yWithinStrip;
    const h = 14;
    const r = rect(svg, {
        x, y, width: labelW, height: h,
        class: 'label-rect',
    });
    text(svg, x + 4, y + 10, name, { 'font-size': 8 });
    setupYDrag(r, fieldKey);
}

// ── BT renderer ────────────────────────────────────────────────────────────

function renderBt(svg) {
    const b = state.bt;
    const W = state.meta.screen_w;

    if (b.bt_show_circle) {
        // Circle as a free element with rounded SVG (preview only)
        const r = rect(svg, {
            x: b.bt_circle_x, y: b.bt_circle_y,
            width: b.bt_circle_w, height: b.bt_circle_h,
            rx: Math.min(b.bt_circle_w, b.bt_circle_h) / 2,
            ry: Math.min(b.bt_circle_w, b.bt_circle_h) / 2,
            class: 'panel',
        });
        setupMove(r, svg, { x: 'bt_circle_x', y: 'bt_circle_y' });
        addCornerHandles(svg,
            b.bt_circle_x, b.bt_circle_y, b.bt_circle_w, b.bt_circle_h,
            { x: 'bt_circle_x', y: 'bt_circle_y',
              w: 'bt_circle_w', h: 'bt_circle_h' });
        // Centered "BT" text
        const cx = b.bt_circle_x + b.bt_circle_w / 2;
        const cy = b.bt_circle_y + b.bt_circle_h / 2;
        const fh = fontHeight(b.bt_icon_font);
        text(svg, cx, cy + fh * 0.35, 'BT', {
            'font-size': fh, 'text-anchor': 'middle',
        });
        tag(svg, b.bt_circle_x + 2, b.bt_circle_y + 7, 'circle');
    }

    drawLabel(svg, b.bt_brand_x, b.bt_brand_y, b.bt_brand_font, 'Bluetooth Audio',
              'brand', { x: 'bt_brand_x', y: 'bt_brand_y' });
    drawLabel(svg, b.bt_status_x, b.bt_status_y, b.bt_status_font, 'Connected',
              'status', { x: 'bt_status_x', y: 'bt_status_y' });

    drawFreeElement(svg, {
        x: b.bt_slider_x, y: b.bt_slider_y,
        w: b.bt_slider_w, h: b.bt_slider_h,
        label: 'slider', cls: 'slider-rect',
        fields: { x: 'bt_slider_x', y: 'bt_slider_y',
                  w: 'bt_slider_w', h: 'bt_slider_h' },
    });

    drawLabel(svg, b.bt_vol_label_x, b.bt_vol_label_y, b.bt_vol_label_font, 'Volume',
              'vol', { x: 'bt_vol_label_x', y: 'bt_vol_label_y' });

    if (b.bt_show_mode_indicator) {
        drawFreeElement(svg, {
            x: b.bt_mode_indic_x, y: b.bt_mode_indic_y, w: 16, h: 16,
            label: 'mode', cls: 'label-rect',
            fields: { x: 'bt_mode_indic_x', y: 'bt_mode_indic_y' },
        });
    }
    if (b.bt_show_clock) {
        // clock_widget — "00:00" label, font 18 (~50 px wide)
        drawFreeElement(svg, {
            x: b.bt_clock_widget_x, y: b.bt_clock_widget_y, w: 50, h: 18,
            label: 'clock', cls: 'label-rect',
            fields: { x: 'bt_clock_widget_x', y: 'bt_clock_widget_y' },
            text: '00:00', textSize: 18,
        });
    }
}

// ── RADIO renderer ─────────────────────────────────────────────────────────

function renderRadio(svg) {
    const r = state.radio;
    const W = state.meta.screen_w;

    if (r.radio_show_np) {
        // Now-playing widget = two stacked labels (station + title, +26px gap).
        // Width is fixed in firmware to screen_w - 20 (full-screen scrolling line).
        const stationFh = 18;  // hardcoded font_18 in now_playing_widget.c
        const titleFh   = 14;  // hardcoded font_14
        const npW       = Math.max(W - 20, 8);
        drawFreeElement(svg, {
            x: r.radio_np_x, y: r.radio_np_y, w: npW, h: stationFh,
            label: 'np_station', cls: 'label-rect',
            fields: { x: 'radio_np_x', y: 'radio_np_y' },
            text: 'Atlas Radio', textSize: stationFh,
        });
        drawFreeElement(svg, {
            x: r.radio_np_x, y: r.radio_np_y + 26, w: npW, h: titleFh,
            label: 'np_title', cls: 'label-rect',
            fields: { x: 'radio_np_x', y: 'radio_np_y' },
            text: 'Title — Artist', textSize: titleFh,
        });
    }

    drawLabel(svg, r.radio_state_x, r.radio_state_y, r.radio_state_font, 'PLAYING',
              'state', { x: 'radio_state_x', y: 'radio_state_y' });
    drawLabel(svg, r.radio_audio_info_x, r.radio_audio_info_y, r.radio_audio_info_font,
              '44100 Hz  2ch  128kbps',
              'info', { x: 'radio_audio_info_x', y: 'radio_audio_info_y' });

    drawFreeElement(svg, {
        x: r.radio_slider_x, y: r.radio_slider_y,
        w: r.radio_slider_w, h: r.radio_slider_h,
        label: 'slider', cls: 'slider-rect',
        fields: { x: 'radio_slider_x', y: 'radio_slider_y',
                  w: 'radio_slider_w', h: 'radio_slider_h' },
    });

    drawLabel(svg, r.radio_vol_label_x, r.radio_vol_label_y, r.radio_vol_label_font,
              '50%', 'vol', { x: 'radio_vol_label_x', y: 'radio_vol_label_y' });

    if (r.radio_show_mode_indicator) {
        drawFreeElement(svg, {
            x: r.radio_mode_indic_x, y: r.radio_mode_indic_y, w: 16, h: 16,
            label: 'mode', cls: 'label-rect',
            fields: { x: 'radio_mode_indic_x', y: 'radio_mode_indic_y' },
        });
    }
    if (r.radio_show_clock) {
        drawFreeElement(svg, {
            x: r.radio_clock_widget_x, y: r.radio_clock_widget_y, w: 50, h: 18,
            label: 'clock', cls: 'label-rect',
            fields: { x: 'radio_clock_widget_x', y: 'radio_clock_widget_y' },
            text: '00:00', textSize: 18,
        });
    }
}

function drawLabel(svg, x, y, fontId, text_str, name, fields) {
    const fh = fontHeight(fontId);
    const tw = Math.round(fh * 0.55) * Math.max(text_str.length, 5);
    drawFreeElement(svg, {
        x, y, w: tw, h: fh,
        label: name, cls: 'label-rect',
        fields,
        text: text_str, textSize: fh,
    });
}

// ── Free element (move + 4 corner resize) ──────────────────────────────────

function drawFreeElement(svg, opts) {
    const r = rect(svg, {
        x: opts.x, y: opts.y, width: opts.w, height: opts.h, class: opts.cls,
    });
    setupMove(r, svg, opts.fields);

    tag(svg, opts.x + 2, opts.y + 7, opts.label);

    if (opts.text) {
        text(svg, opts.x + opts.w / 2, opts.y + opts.h * 0.78, opts.text, {
            'font-size': Math.min(opts.textSize, opts.h),
            'text-anchor': 'middle',
        });
    }

    if (opts.fields.w && opts.fields.h) {
        addCornerHandles(svg, opts.x, opts.y, opts.w, opts.h, opts.fields);
    }
}

function addCornerHandles(svg, x, y, w, h, fields) {
    const corners = [
        { cx: x,     cy: y,     dir: 'tl' },
        { cx: x + w, cy: y,     dir: 'tr' },
        { cx: x,     cy: y + h, dir: 'bl' },
        { cx: x + w, cy: y + h, dir: 'br' },
    ];
    for (const cr of corners) {
        const handle = rect(svg, {
            x: cr.cx - HANDLE_SIZE / 2,
            y: cr.cy - HANDLE_SIZE / 2,
            width: HANDLE_SIZE, height: HANDLE_SIZE,
            class: 'corner-handle',
            'data-corner': cr.dir,
        });
        setupResize(handle, svg, fields, cr.dir);
    }
}

function setupMove(el, svg, fields) {
    el.addEventListener('pointerdown', (e) => {
        if (e.target !== el) return;
        e.preventDefault();
        el.setPointerCapture(e.pointerId);
        el.classList.add('dragging');

        const data = state[state.active];
        const start = { mx: e.clientX, my: e.clientY,
                        x: data[fields.x] | 0, y: data[fields.y] | 0 };
        const px = svg.getBoundingClientRect();
        const pxPerU = { x: px.width / state.meta.screen_w,
                         y: px.height / state.meta.screen_h };

        const onMove = (ev) => {
            const dx = Math.round((ev.clientX - start.mx) / pxPerU.x);
            const dy = Math.round((ev.clientY - start.my) / pxPerU.y);
            data[fields.x] = start.x + dx;
            data[fields.y] = start.y + dy;
            setFormValue(fields.x, data[fields.x]);
            setFormValue(fields.y, data[fields.y]);
            renderSvg();
        };
        const onUp = () => {
            window.removeEventListener('pointermove', onMove);
            window.removeEventListener('pointerup', onUp);
            el.classList.remove('dragging');
        };
        window.addEventListener('pointermove', onMove);
        window.addEventListener('pointerup', onUp);
    });
}

function setupResize(el, svg, fields, dir) {
    el.addEventListener('pointerdown', (e) => {
        e.preventDefault();
        e.stopPropagation();
        el.setPointerCapture(e.pointerId);

        const data = state[state.active];
        const start = {
            mx: e.clientX, my: e.clientY,
            x: data[fields.x] | 0, y: data[fields.y] | 0,
            w: data[fields.w] | 0, h: data[fields.h] | 0,
        };
        const px = svg.getBoundingClientRect();
        const pxPerU = { x: px.width / state.meta.screen_w,
                         y: px.height / state.meta.screen_h };

        const onMove = (ev) => {
            const dx = Math.round((ev.clientX - start.mx) / pxPerU.x);
            const dy = Math.round((ev.clientY - start.my) / pxPerU.y);
            let nx = start.x, ny = start.y, nw = start.w, nh = start.h;
            if (dir.includes('l')) { nx = start.x + dx; nw = start.w - dx; }
            if (dir.includes('r')) {                     nw = start.w + dx; }
            if (dir.includes('t')) { ny = start.y + dy; nh = start.h - dy; }
            if (dir.includes('b')) {                     nh = start.h + dy; }
            if (nw < 4) { nw = 4; if (dir.includes('l')) nx = start.x + start.w - 4; }
            if (nh < 4) { nh = 4; if (dir.includes('t')) ny = start.y + start.h - 4; }

            data[fields.x] = nx; data[fields.y] = ny;
            data[fields.w] = nw; data[fields.h] = nh;
            setFormValue(fields.x, nx); setFormValue(fields.y, ny);
            setFormValue(fields.w, nw); setFormValue(fields.h, nh);
            renderSvg();
        };
        const onUp = () => {
            window.removeEventListener('pointermove', onMove);
            window.removeEventListener('pointerup', onUp);
        };
        window.addEventListener('pointermove', onMove);
        window.addEventListener('pointerup', onUp);
    });
}

// ── Y-only drag (used by strip station/title labels) ──────────────────────

function setupYDrag(el, field) {
    el.addEventListener('pointerdown', (e) => {
        e.preventDefault();
        e.stopPropagation();
        el.setPointerCapture(e.pointerId);
        el.classList.add('dragging');

        const data = state[state.active];
        const startY = e.clientY;
        const startVal = data[field] | 0;
        const pxPerU = el.ownerSVGElement.getBoundingClientRect().height
                       / state.meta.screen_h;

        const onMove = (ev) => {
            const dy = Math.round((ev.clientY - startY) / pxPerU);
            const v = startVal + dy;
            if (v !== data[field]) {
                data[field] = v;
                setFormValue(field, v);
                renderSvg();
            }
        };
        const onUp = () => {
            window.removeEventListener('pointermove', onMove);
            window.removeEventListener('pointerup', onUp);
            el.classList.remove('dragging');
        };
        window.addEventListener('pointermove', onMove);
        window.addEventListener('pointerup', onUp);
    });
}

// ── Apply / reset ──────────────────────────────────────────────────────────

async function applyProfile() {
    const btn = document.getElementById('btn_apply');
    btn.disabled = true;
    setStatus('Applying...');
    try {
        const r = await fetch(`/api/ui/profile/${state.active}`, {
            method:  'POST',
            headers: { 'Content-Type': 'application/json' },
            body:    JSON.stringify(state[state.active]),
        });
        if (!r.ok) throw new Error('HTTP ' + r.status);
        setStatus(`Applied (${state.active}) — device screen rebuilding.`);
    } catch (err) {
        setStatus('Apply failed: ' + err.message, true);
    } finally {
        btn.disabled = false;
    }
}

async function resetProfile() {
    if (!confirm('Reset ALL layout sections to factory defaults?')) return;
    setStatus('Resetting...');
    try {
        await fetch('/api/ui/profile/reset', { method: 'POST' });
        for (const name of Object.keys(SECTIONS)) {
            state[name] = await fetch(`/api/ui/profile/${name}`).then(r => r.json());
        }
        // refresh form/svg of the active section
        for (const f of SECTIONS[state.active].fields) {
            setFormValue(f.key, state[state.active][f.key]);
        }
        renderSvg();
        setStatus('Reset to defaults.');
    } catch (err) {
        setStatus('Reset failed: ' + err.message, true);
    }
}

// ── helpers ─────────────────────────────────────────────────────────────────

function fontHeight(id) {
    if (!id) return 14;
    const m = id.match(/_(\d+)(_pl)?$/);
    return m ? parseInt(m[1], 10) : 14;
}

function rect(parent, attrs) {
    const r = document.createElementNS(SVG_NS, 'rect');
    for (const k in attrs) r.setAttribute(k, attrs[k]);
    parent.appendChild(r);
    return r;
}
function text(parent, x, y, txt, attrs = {}) {
    const t = document.createElementNS(SVG_NS, 'text');
    t.setAttribute('x', x); t.setAttribute('y', y);
    t.setAttribute('class', 'lcd-text');
    for (const k in attrs) t.setAttribute(k, attrs[k]);
    t.textContent = txt;
    parent.appendChild(t);
    return t;
}
function tag(parent, x, y, txt) {
    const t = document.createElementNS(SVG_NS, 'text');
    t.setAttribute('x', x); t.setAttribute('y', y);
    t.setAttribute('class', 'field-tag');
    t.textContent = txt;
    parent.appendChild(t);
}
function clamp(v, lo, hi) { return Math.max(lo, Math.min(hi, v | 0)); }
function setStatus(msg, error = false) {
    const el = document.getElementById('status_msg');
    el.textContent = msg;
    el.style.color = error ? 'var(--red)' : 'var(--text-dim)';
}
