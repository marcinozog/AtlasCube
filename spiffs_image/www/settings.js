'use strict';

// ─────────────────────────────────────────────────────────────────────────────
// State
// ─────────────────────────────────────────────────────────────────────────────
let currentSettings = null;
let isApMode        = false;
let brightnessTimeout;
let scrsDelayTimeout;
let dashboardTimeout;

// ─────────────────────────────────────────────────────────────────────────────
// Device theme (display)
// ─────────────────────────────────────────────────────────────────────────────
function setDeviceTheme(t) {
    document.getElementById('settingsBtnDark') ?.classList.toggle('active', t === 'dark');
    document.getElementById('settingsBtnLight')?.classList.toggle('active', t === 'light');
    fetch('/api/settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ display: { theme: t } })
    }).catch(console.error);
}

// ─────────────────────────────────────────────────────────────────────────────
// Brightness (display)
// ─────────────────────────────────────────────────────────────────────────────
function setDisplayBrightness(t) {
    document.getElementById('disp_bright_value').innerText = t;
    clearTimeout(brightnessTimeout);
    brightnessTimeout = setTimeout(() => {
        fetch('/api/settings', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ display: { brightness: parseInt(t) } })
        }).catch(console.error);
    }, 300);
}

// ─────────────────────────────────────────────────────────────────────────────
// Bluetooth screen (display)
// ─────────────────────────────────────────────────────────────────────────────
function setDeviceBTScreen(t) {
    document.getElementById('settingsBtnBTshow') ?.classList.toggle('active', t);
    document.getElementById('settingsBtnBThide')?.classList.toggle('active', !t);
    fetch('/api/settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ bluetooth: { show_screen: t } })
    }).catch(console.error);
}

// ─────────────────────────────────────────────────────────────────────────────
// Audio — DSP / EQ on/off
// ─────────────────────────────────────────────────────────────────────────────
function setDeviceEqEnabled(t) {
    document.getElementById('settingsBtnDspOn') ?.classList.toggle('active', t);
    document.getElementById('settingsBtnDspOff')?.classList.toggle('active', !t);
    fetch('/api/settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ audio: { eq_enabled: t } })
    }).catch(console.error);
}

// ─────────────────────────────────────────────────────────────────────────────
// Screensaver
// ─────────────────────────────────────────────────────────────────────────────
function setScrsaverDelay(t) {
    clearTimeout(scrsDelayTimeout);
    scrsDelayTimeout = setTimeout(() => {
        let v = parseInt(t, 10);
        if (isNaN(v) || v < 0) v = 0;
        fetch('/api/settings', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ scrsaver: { delay: v } })
        }).catch(console.error);
    }, 400);
}

function setScrsaverId(id) {
    fetch('/api/settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ scrsaver: { id } })
    }).catch(console.error);
}

// ─────────────────────────────────────────────────────────────────────────────
// Dashboard screensaver widget — debounced full-form push
// ─────────────────────────────────────────────────────────────────────────────
function setDashboard() {
    clearTimeout(dashboardTimeout);
    dashboardTimeout = setTimeout(() => {
        const title     = (document.getElementById('dash_title')?.value     ?? '').trim();
        const url       = (document.getElementById('dash_url')?.value       ?? '').trim();
        const json_path = (document.getElementById('dash_json_path')?.value ?? '').trim();
        const suffix    =  document.getElementById('dash_suffix')?.value    ?? '';
        let   pollS     = parseInt(document.getElementById('dash_poll_s')?.value, 10);
        if (isNaN(pollS) || pollS < 5) pollS = 5;
        fetch('/api/settings', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ dashboard: {
                title, url, json_path, suffix,
                poll_interval_ms: pollS * 1000
            }})
        }).catch(console.error);
    }, 500);
}

// ─────────────────────────────────────────────────────────────────────────────
// Dashboard notification
// ─────────────────────────────────────────────────────────────────────────────
let dashNotifyTimeout;

function setDashboardNotifyEnabled(t) {
    document.getElementById('dashNotifyOn') ?.classList.toggle('active', t);
    document.getElementById('dashNotifyOff')?.classList.toggle('active', !t);
    document.getElementById('dash_notify_panel').style.display = t ? '' : 'none';
    fetch('/api/settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ dashboard: { notify: { enabled: t } } })
    }).catch(console.error);
}

function onDashboardValueTypeChange() {
    const t = document.getElementById('dash_value_type').value;
    document.getElementById('dash_notify_num').style.display = (t === 'number') ? '' : 'none';
    document.getElementById('dash_notify_str').style.display = (t === 'string') ? '' : 'none';
    setDashboardNotify();
}

function setDashboardNotify() {
    clearTimeout(dashNotifyTimeout);
    dashNotifyTimeout = setTimeout(() => {
        const get = id => document.getElementById(id);
        const num = id => {
            const v = parseFloat(get(id)?.value);
            return isNaN(v) ? 0 : v;
        };
        const notify = {
            value_type:  get('dash_value_type').value,
            num_low_en:  get('dash_num_low_en').checked,
            num_low:     num('dash_num_low'),
            num_high_en: get('dash_num_high_en').checked,
            num_high:    num('dash_num_high'),
            str_eq_en:   get('dash_str_eq_en').checked,
            str_eq:      get('dash_str_eq').value,
            str_ne_en:   get('dash_str_ne_en').checked,
            str_ne:      get('dash_str_ne').value,
        };
        fetch('/api/settings', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ dashboard: { notify } })
        }).catch(console.error);
    }, 500);
}

// ─────────────────────────────────────────────────────────────────────────────
// WiFi — show/hide password
// ─────────────────────────────────────────────────────────────────────────────
function togglePwVisibility() {
    const inp = document.getElementById('wifi_password');
    const eye = document.getElementById('pw_eye');
    if (inp.type === 'password') {
        inp.type = 'text';
        eye.style.opacity = '1';
    } else {
        inp.type = 'password';
        eye.style.opacity = '0.45';
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// WiFi — save & restart
// ─────────────────────────────────────────────────────────────────────────────
async function saveWifi() {
    const ssid = document.getElementById('wifi_ssid').value.trim();
    const pass = document.getElementById('wifi_password').value;

    if (!ssid) {
        showWifiStatus('❌ SSID cannot be empty.', 'error');
        return;
    }

    const btn = document.getElementById('wifi_save_btn');
    btn.disabled = true;
    showWifiStatus('💾 Saving…', '');

    try {
        const payload = { wifi: { ssid } };
        if (pass) payload.wifi.password = pass;   // empty → keep old

        const res = await fetch('/api/settings', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload),
        });
        const data = await res.json();
        if (!data.ok) throw new Error('Device error');

        showWifiStatus('✅ Saved! Device will restart in 3 seconds…', 'ok');

        // Start restart after a short delay
        setTimeout(async () => {
            try {
                await fetch('/api/restart', { method: 'POST' });
            } catch (_) {
                // After restarting the connection will be lost
            }
            showWifiStatus(
                '🔄 The device is restarting. Connect to a WiFi network. ' +
                '"' + ssid + '" and find the IP address of the device in the router.',
                'ok'
            );
        }, 3000);

    } catch (e) {
        showWifiStatus('❌ Error: ' + e.message, 'error');
        btn.disabled = false;
    }
}

let _wifiStatusTimer = null;
function showWifiStatus(msg, type) {
    const el = document.getElementById('wifi_status');
    if (!el) return;
    el.innerText = msg;
    el.className = 'save-status' + (type ? ' ' + type : '');
    clearTimeout(_wifiStatusTimer);
}

// ─────────────────────────────────────────────────────────────────────────────
// Load — fetch /api/state (wifi mode) and /api/settings (data)
// ─────────────────────────────────────────────────────────────────────────────
async function loadSettings() {
    showStatus('Loading…', '');
    try {
        // Check WiFi mode
        const stateRes = await fetch('/api/state', { cache: 'no-store' });
        if (stateRes.ok) {
            const state = await stateRes.json();
            isApMode = state.wifi_mode === 'ap';
            showApBanner(isApMode);
        }

        // Load settings
        const res = await fetch('/api/settings', { cache: 'no-store' });
        if (!res.ok) throw new Error('HTTP ' + res.status);
        currentSettings = await res.json();
        populateForm(currentSettings);
        showStatus('', '');
    } catch (e) {
        showStatus('❌ Could not load settings: ' + e.message, 'error');
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// AP mode banner
// ─────────────────────────────────────────────────────────────────────────────
function showApBanner(ap) {
    const el = document.getElementById('wifi_mode_banner');
    if (!el) return;
    if (ap) {
        el.innerHTML =
            '⚠️ The device operates in Access Point mode.<br>' +
            'Enter your WiFi network details and click <strong>Save WiFi and restart</strong>.';
        el.classList.remove('hidden');
    } else {
        el.innerHTML = '✅ Connected to WiFi network.';
        el.classList.remove('hidden');
        el.classList.add('connected');
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Populate form
// ─────────────────────────────────────────────────────────────────────────────
function populateForm(s) {
    if (!s) return;
    if (s.wifi) {
        setVal('wifi_ssid', s.wifi.ssid || '');
        // password is never sent back — field stays empty
    }
    if (s.ntp) {
        setVal('ntp_server1', s.ntp.server1 || '');
        setVal('ntp_server2', s.ntp.server2 || '');
        setVal('ntp_tz',      s.ntp.tz      || '');
        syncPresetDropdown(s.ntp.tz || '');
    }
    if (s.display) {
        const t = s.display.theme || 'dark';
        document.getElementById('settingsBtnDark') ?.classList.toggle('active', t === 'dark');
        document.getElementById('settingsBtnLight')?.classList.toggle('active', t === 'light');

        if (s.display.brightness !== undefined) {
            const b = s.display.brightness;
            document.getElementById('disp_bright_slider').value = b;
            document.getElementById('disp_bright_value').innerText = b;
        }
    }
    if (s.bluetooth) {
        const t = s.bluetooth.show_screen || false;
        document.getElementById('settingsBtnBTshow') ?.classList.toggle('active', t);
        document.getElementById('settingsBtnBThide')?.classList.toggle('active', !t);
    }
    if (s.audio) {
        // default true — when older backend does not return eq_enabled
        const eq_en = (s.audio.eq_enabled !== false);
        document.getElementById('settingsBtnDspOn') ?.classList.toggle('active', eq_en);
        document.getElementById('settingsBtnDspOff')?.classList.toggle('active', !eq_en);
    }
    if (s.scrsaver) {
        setVal('scrs_delay', s.scrsaver.delay ?? 60);
        const sel = document.getElementById('scrs_id');
        if (sel) sel.value = s.scrsaver.id || 'clockhands';
    }
    if (s.dashboard) {
        setVal('dash_title',     s.dashboard.title     ?? '');
        setVal('dash_url',       s.dashboard.url       ?? '');
        setVal('dash_json_path', s.dashboard.json_path ?? '');
        setVal('dash_suffix',    s.dashboard.suffix    ?? '');
        setVal('dash_poll_s', Math.max(5, Math.round((s.dashboard.poll_interval_ms ?? 60000) / 1000)));

        const n = s.dashboard.notify || {};
        const enabled = !!n.enabled;
        document.getElementById('dashNotifyOn') ?.classList.toggle('active', enabled);
        document.getElementById('dashNotifyOff')?.classList.toggle('active', !enabled);
        document.getElementById('dash_notify_panel').style.display = enabled ? '' : 'none';

        const vt = n.value_type === 'string' ? 'string' : 'number';
        setVal('dash_value_type', vt);
        document.getElementById('dash_notify_num').style.display = (vt === 'number') ? '' : 'none';
        document.getElementById('dash_notify_str').style.display = (vt === 'string') ? '' : 'none';

        const cb = (id, v) => { const el = document.getElementById(id); if (el) el.checked = !!v; };
        cb('dash_num_low_en',  n.num_low_en);
        cb('dash_num_high_en', n.num_high_en);
        cb('dash_str_eq_en',   n.str_eq_en);
        cb('dash_str_ne_en',   n.str_ne_en);
        setVal('dash_num_low',  n.num_low  ?? '');
        setVal('dash_num_high', n.num_high ?? '');
        setVal('dash_str_eq',   n.str_eq   ?? '');
        setVal('dash_str_ne',   n.str_ne   ?? '');
    }
}

function setVal(id, value) {
    const el = document.getElementById(id);
    if (el) el.value = value;
}

// ─────────────────────────────────────────────────────────────────────────────
// TZ preset
// ─────────────────────────────────────────────────────────────────────────────
function syncPresetDropdown(tzString) {
    const sel = document.getElementById('tz_preset');
    if (!sel) return;
    let matched = false;
    for (const opt of sel.options) {
        if (opt.value === tzString) { opt.selected = true; matched = true; break; }
    }
    if (!matched) sel.value = '';
}

function onTzPreset(value) { if (value) setVal('ntp_tz', value); }
function onTzManualEdit()  { syncPresetDropdown(document.getElementById('ntp_tz').value); }

// ─────────────────────────────────────────────────────────────────────────────
// Save NTP + Display
// ─────────────────────────────────────────────────────────────────────────────
async function saveSettings() {
    const btn = document.getElementById('save_btn');
    btn.disabled = true;

    const darkActive = document.getElementById('settingsBtnDark')?.classList.contains('active');
    const payload = {
        ntp: {
            server1: document.getElementById('ntp_server1').value.trim(),
            server2: document.getElementById('ntp_server2').value.trim(),
            tz:      document.getElementById('ntp_tz').value.trim(),
        },
        display: { theme: darkActive ? 'dark' : 'light' }
    };

    if (!payload.ntp.server1) { showStatus('❌ NTP Server 1 cannot be empty', 'error'); btn.disabled = false; return; }
    if (!payload.ntp.tz)      { showStatus('❌ Timezone cannot be empty', 'error');      btn.disabled = false; return; }
    if (payload.ntp.tz.length > 63) { showStatus('❌ TZ too long (max 63 characters)', 'error'); btn.disabled = false; return; }

    try {
        const res  = await fetch('/api/settings', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload),
        });
        const data = await res.json();
        if (data.ok) {
            showStatus('✅ Saved. NTP will synchronize shortly.', 'ok');
        } else {
            showStatus('❌ The device returned an error', 'error');
        }
    } catch (e) {
        showStatus('❌ Connection error: ' + e.message, 'error');
    } finally {
        btn.disabled = false;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Status bar
// ─────────────────────────────────────────────────────────────────────────────
let _statusTimer = null;
function showStatus(msg, type) {
    const el = document.getElementById('save_status');
    if (!el) return;
    el.innerText = msg;
    el.className = 'save-status' + (type ? ' ' + type : '');
    clearTimeout(_statusTimer);
    if (type === 'ok') _statusTimer = setTimeout(() => { el.innerText = ''; el.className = 'save-status'; }, 5000);
}

// ─────────────────────────────────────────────────────────────────────────────
// Colors (custom palettes) — /api/theme
// ─────────────────────────────────────────────────────────────────────────────
const COLOR_FIELDS = [
    ['bg_primary',     'Screen background',   'Main background of all screens'],
    ['bg_secondary',   'Panels & cards',      'Strips, cards, sliders track'],
    ['text_primary',   'Main text',           'Clock digits, titles, main labels'],
    ['text_secondary', 'Secondary text',      'Date, station name in strip, volume %'],
    ['text_muted',     'Subtle text',         'Audio info, ICY title, "Syncing…"'],
    ['accent',         'Accent / highlight',  'Radio station name, slider fill'],
    ['bt_brand',       'Bluetooth',           'BT icon, "Bluetooth Audio", BT slider'],
    ['status_ok',      'Status OK',           'Connected / Playing (semantic green)'],
];

let colorData     = { dark: {}, light: {} };
let activePalette = 'dark';
let colorSaveTimeout = null;

async function loadColors() {
    try {
        const res = await fetch('/api/theme', { cache: 'no-store' });
        if (!res.ok) throw new Error('HTTP ' + res.status);
        const data = await res.json();
        colorData.dark  = data.dark  || {};
        colorData.light = data.light || {};
        renderColorGrid();
    } catch (e) {
        console.error('loadColors', e);
        showColorsStatus('❌ Could not load colors: ' + e.message, 'error');
    }
}

function selectPaletteTab(which) {
    activePalette = which;
    document.getElementById('palTabDark') ?.classList.toggle('active', which === 'dark');
    document.getElementById('palTabLight')?.classList.toggle('active', which === 'light');
    renderColorGrid();
}

function renderColorGrid() {
    const grid = document.getElementById('colorGrid');
    if (!grid) return;
    const pal = colorData[activePalette] || {};
    grid.innerHTML = COLOR_FIELDS.map(([key, label, hint]) => {
        const v = (pal[key] || '#000000').toUpperCase();
        return `
        <div class="color-row">
            <div class="color-label-wrap">
                <label class="color-label">${label}</label>
                <div class="color-hint">${hint}</div>
            </div>
            <input type="color" id="col_${key}" value="${v}"
                   oninput="onColorChange('${key}', this.value)" class="color-swatch" />
            <input type="text" id="hex_${key}" value="${v}" maxlength="7"
                   oninput="onHexChange('${key}', this.value)" class="field-input color-hex" />
        </div>`;
    }).join('');
}

function onColorChange(key, val) {
    colorData[activePalette][key] = val;
    const hex = document.getElementById('hex_' + key);
    if (hex) hex.value = val.toUpperCase();
}

function onHexChange(key, val) {
    if (!/^#[0-9a-fA-F]{6}$/.test(val)) return;
    colorData[activePalette][key] = val;
    const col = document.getElementById('col_' + key);
    if (col) col.value = val;
}

async function saveColors() {
    const btn = document.getElementById('colors_save_btn');
    btn.disabled = true;
    try {
        const payload = {};
        payload[activePalette] = colorData[activePalette];
        const res = await fetch('/api/theme', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload),
        });
        const data = await res.json();
        if (data.ok) showColorsStatus('✅ Palette saved', 'ok');
        else         showColorsStatus('❌ Device returned error', 'error');
    } catch (e) {
        showColorsStatus('❌ ' + e.message, 'error');
    } finally {
        btn.disabled = false;
    }
}

async function resetColors() {
    if (!confirm('Reset "' + activePalette + '" palette to factory defaults?')) return;
    const btn = document.getElementById('colors_reset_btn');
    btn.disabled = true;
    try {
        const res = await fetch('/api/theme', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ reset: activePalette }),
        });
        const data = await res.json();
        if (data.ok) {
            await loadColors();
            showColorsStatus('✅ Reset done', 'ok');
        } else {
            showColorsStatus('❌ Device returned error', 'error');
        }
    } catch (e) {
        showColorsStatus('❌ ' + e.message, 'error');
    } finally {
        btn.disabled = false;
    }
}

let _colorsStatusTimer = null;
function showColorsStatus(msg, type) {
    const el = document.getElementById('colors_status');
    if (!el) return;
    el.innerText = msg;
    el.className = 'save-status' + (type ? ' ' + type : '');
    clearTimeout(_colorsStatusTimer);
    if (type === 'ok') _colorsStatusTimer = setTimeout(
        () => { el.innerText = ''; el.className = 'save-status'; }, 4000);
}

// ─────────────────────────────────────────────────────────────────────────────
// Init
// ─────────────────────────────────────────────────────────────────────────────
loadSettings();
loadColors();