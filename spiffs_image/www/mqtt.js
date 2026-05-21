'use strict';

const MQTT_MAX_WIDGETS = 6;
let _cfg = null;

function el(tag, attrs = {}, ...children) {
    const e = document.createElement(tag);
    for (const k in attrs) {
        if (k === 'class')       e.className = attrs[k];
        else if (k === 'style')  e.style.cssText = attrs[k];
        else if (k.startsWith('on')) e.addEventListener(k.slice(2), attrs[k]);
        else                     e.setAttribute(k, attrs[k]);
    }
    for (const c of children) {
        if (c == null) continue;
        e.appendChild(typeof c === 'string' ? document.createTextNode(c) : c);
    }
    return e;
}

function renderTypeFields(card, idx, type) {
    const cmdRow    = card.querySelector('[data-row="cmd"]');
    const stateRow  = card.querySelector('[data-row="state"]');
    const jsonRow   = card.querySelector('[data-row="json"]');
    const rangeRow  = card.querySelector('[data-row="range"]');
    const unitRow   = card.querySelector('[data-row="unit"]');

    const show = (e, on) => { if (e) e.style.display = on ? '' : 'none'; };

    show(cmdRow,   type === 'toggle' || type === 'slider');
    show(stateRow, type === 'toggle' || type === 'slider' || type === 'label');
    show(jsonRow,  type === 'toggle' || type === 'slider' || type === 'label');
    show(rangeRow, type === 'slider');
    show(unitRow,  type === 'label'  || type === 'slider');
}

function buildCard(idx, w) {
    const w2 = w || { type: 'none', title: '', topic_cmd: '', topic_state: '',
                       json_path: '', unit: '', min: 0, max: 100, step: 1 };

    const card = el('div', { class: 'panel', 'data-idx': idx, style: 'margin-bottom:12px' });

    card.appendChild(el('div', { class: 'field-label', style: 'font-weight:700' }, `Slot ${idx + 1}`));

    // Type
    const sel = el('select', { class: 'field-input', id: `w_type_${idx}`,
                                style: 'width:180px',
                                onchange: (e) => renderTypeFields(card, idx, e.target.value) });
    for (const [v, label] of [['none','— None —'], ['toggle','🔘 Toggle'],
                              ['slider','🎚 Slider'], ['label','🏷 Label']]) {
        const opt = el('option', { value: v }, label);
        if (v === w2.type) opt.selected = true;
        sel.appendChild(opt);
    }
    card.appendChild(el('div', { class: 'field-group' },
        el('label', { class: 'field-label' }, 'Type'), sel));

    // Title
    card.appendChild(el('div', { class: 'field-group' },
        el('label', { class: 'field-label' }, 'Title'),
        el('input', { type: 'text', id: `w_title_${idx}`, class: 'field-input',
                      maxlength: 23, value: w2.title || '' })));

    // Command topic
    card.appendChild(el('div', { class: 'field-group', 'data-row': 'cmd' },
        el('label', { class: 'field-label' }, 'Command topic'),
        el('input', { type: 'text', id: `w_cmd_${idx}`, class: 'field-input',
                      placeholder: 'home/light/cmnd/POWER',
                      value: w2.topic_cmd || '' }),
        el('div', { class: 'field-hint' },
            'Publish on widget interaction. Toggle: ', el('code', {}, 'ON'), '/', el('code', {}, 'OFF'),
            '. Slider: integer.')));

    // State topic
    card.appendChild(el('div', { class: 'field-group', 'data-row': 'state' },
        el('label', { class: 'field-label' }, 'State topic'),
        el('input', { type: 'text', id: `w_state_${idx}`, class: 'field-input',
                      placeholder: 'home/light/stat/POWER',
                      value: w2.topic_state || '' }),
        el('div', { class: 'field-hint' },
            'Subscribed; updates the widget to reflect the real device state.')));

    // JSON path
    card.appendChild(el('div', { class: 'field-group', 'data-row': 'json' },
        el('label', { class: 'field-label' }, 'JSON path (optional)'),
        el('input', { type: 'text', id: `w_json_${idx}`, class: 'field-input',
                      placeholder: 'state', maxlength: 47,
                      value: w2.json_path || '' }),
        el('div', { class: 'field-hint' },
            'Top-level key to extract from JSON payloads (e.g. ',
            el('code', {}, 'state'), ' for zigbee2mqtt). Leave empty if the payload is plain text.')));

    // Range (slider only)
    const range = el('div', { class: 'field-group', 'data-row': 'range',
                              style: 'display:flex; gap:12px; flex-wrap:wrap' });
    range.appendChild(el('div', {},
        el('label', { class: 'field-label' }, 'Min'),
        el('input', { type: 'number', id: `w_min_${idx}`, class: 'field-input',
                      style: 'width:90px', value: w2.min ?? 0 })));
    range.appendChild(el('div', {},
        el('label', { class: 'field-label' }, 'Max'),
        el('input', { type: 'number', id: `w_max_${idx}`, class: 'field-input',
                      style: 'width:90px', value: w2.max ?? 100 })));
    range.appendChild(el('div', {},
        el('label', { class: 'field-label' }, 'Step'),
        el('input', { type: 'number', id: `w_step_${idx}`, class: 'field-input',
                      style: 'width:90px', min: 1, value: w2.step ?? 1 })));
    card.appendChild(range);

    // Unit (label / slider)
    card.appendChild(el('div', { class: 'field-group', 'data-row': 'unit' },
        el('label', { class: 'field-label' }, 'Unit (optional)'),
        el('input', { type: 'text', id: `w_unit_${idx}`, class: 'field-input',
                      style: 'width:120px', placeholder: '°C, %, ...',
                      maxlength: 7, value: w2.unit || '' })));

    renderTypeFields(card, idx, w2.type || 'none');
    return card;
}

function renderAll(cfg) {
    const root = document.getElementById('widgets_container');
    root.innerHTML = '';
    const widgets = (cfg && cfg.widgets) || [];
    for (let i = 0; i < MQTT_MAX_WIDGETS; ++i) {
        root.appendChild(buildCard(i, widgets[i]));
    }
}

async function loadAll() {
    try {
        const r = await fetch('/api/mqtt', { cache: 'no-store' });
        if (!r.ok) throw new Error('HTTP ' + r.status);
        _cfg = await r.json();
        renderAll(_cfg);
    } catch (e) {
        showStatus('❌ Could not load: ' + e.message, 'error');
        renderAll({});
    }
}

function collect() {
    const widgets = [];
    for (let i = 0; i < MQTT_MAX_WIDGETS; ++i) {
        const g = id => document.getElementById(id)?.value ?? '';
        const n = id => {
            const v = parseInt(g(id), 10);
            return isNaN(v) ? 0 : v;
        };
        widgets.push({
            type:        g(`w_type_${i}`),
            title:       g(`w_title_${i}`).trim(),
            topic_cmd:   g(`w_cmd_${i}`).trim(),
            topic_state: g(`w_state_${i}`).trim(),
            json_path:   g(`w_json_${i}`).trim(),
            unit:        g(`w_unit_${i}`),
            min:         n(`w_min_${i}`),
            max:         n(`w_max_${i}`),
            step:        n(`w_step_${i}`) || 1,
        });
    }
    return widgets;
}

async function saveWidgets() {
    const btn = document.getElementById('mqtt_widgets_save_btn');
    btn.disabled = true;
    try {
        // Merge widgets into the existing config so we don't drop broker fields
        const payload = { ...(_cfg || {}), widgets: collect() };
        delete payload.password;        // never send echoed (empty) password back
        const r = await fetch('/api/mqtt', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload),
        });
        if (!r.ok) throw new Error('HTTP ' + r.status);
        showStatus('✅ Saved. Client reconnecting…', 'ok');
        await loadAll();   // refresh _cfg to reflect server-side state
    } catch (e) {
        showStatus('❌ ' + e.message, 'error');
    } finally {
        btn.disabled = false;
    }
}

let _statusTimer = null;
function showStatus(msg, type) {
    const el = document.getElementById('mqtt_widgets_status');
    if (!el) return;
    el.innerText = msg;
    el.className = 'save-status' + (type ? ' ' + type : '');
    clearTimeout(_statusTimer);
    if (type === 'ok') _statusTimer = setTimeout(
        () => { el.innerText = ''; el.className = 'save-status'; }, 4000);
}

loadAll();
