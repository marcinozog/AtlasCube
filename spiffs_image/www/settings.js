'use strict';

// ─────────────────────────────────────────────────────────────────────────────
// State
// ─────────────────────────────────────────────────────────────────────────────
let currentSettings = null;
let isApMode        = false;
let brightnessTimeout;

// ─────────────────────────────────────────────────────────────────────────────
// Tabs
// ─────────────────────────────────────────────────────────────────────────────
const TAB_STORAGE_KEY = 'atlascube.settings.tab';
const DEFAULT_TAB     = 'display';

function getKnownTabs() {
    return Array.from(document.querySelectorAll('#settingsTabNav .tab-btn'))
                .map(b => b.dataset.tab);
}

function selectTab(name) {
    const known = getKnownTabs();
    if (!known.includes(name)) name = DEFAULT_TAB;

    for (const btn  of document.querySelectorAll('#settingsTabNav .tab-btn')) {
        btn.classList.toggle('active', btn.dataset.tab === name);
    }
    for (const pane of document.querySelectorAll('.tab-pane')) {
        pane.classList.toggle('active', pane.dataset.tab === name);
    }
    try { localStorage.setItem(TAB_STORAGE_KEY, name); } catch (e) { /* private mode */ }

    // Persist in URL fragment so deep links work without hitting the server
    // with a query string the file handler can't resolve.
    const newHash = '#tab=' + name;
    if (window.location.hash !== newHash) {
        window.history.replaceState(null, '', window.location.pathname + newHash);
    }
}

function tabFromHash() {
    const m = /(?:^|[#&])tab=([^&]+)/.exec(window.location.hash || '');
    return m ? decodeURIComponent(m[1]) : null;
}

function initTabs() {
    const known    = getKnownTabs();
    const hashTab  = tabFromHash();
    let   savedTab = null;
    try { savedTab = localStorage.getItem(TAB_STORAGE_KEY); } catch (e) { /* ignore */ }

    const initial = (hashTab  && known.includes(hashTab))  ? hashTab
                  : (savedTab && known.includes(savedTab)) ? savedTab
                  : DEFAULT_TAB;
    selectTab(initial);

    // React to manual hash edits / back-forward navigation.
    window.addEventListener('hashchange', () => {
        const t = tabFromHash();
        if (t) selectTab(t);
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// Device screen (display) — switch + persist the active/startup screen
// ─────────────────────────────────────────────────────────────────────────────
function setScreen(name) {
    fetch('/api/settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ display: { screen: name } })
    }).catch(console.error);
}

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
// Background mode (display): gradient / solid / wallpaper
// ─────────────────────────────────────────────────────────────────────────────
const SD_MOUNT = '/sdcard';

function setBackground(mode) {
    const grad = mode === 'gradient';
    const wall = mode === 'wallpaper';
    document.getElementById('settingsBtnBgGrad') ?.classList.toggle('active', grad);
    document.getElementById('settingsBtnBgSolid')?.classList.toggle('active', mode === 'solid');
    document.getElementById('settingsBtnBgWall') ?.classList.toggle('active', wall);
    document.getElementById('wallpaperPicker').style.display = wall ? '' : 'none';
    const body = wall ? { display: { wallpaper_on: true } }
                      : { display: { wallpaper_on: false, bg_gradient: grad } };
    fetch('/api/settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body)
    }).catch(console.error);
}

// SD file browser for picking a wallpaper .bin (reuses /api/sd/list).
function openWallpaperBrowser() {
    const box = document.getElementById('wallpaperBrowser');
    box.style.display = '';
    // Start in the folder of the currently selected wallpaper, if any.
    const cur = document.getElementById('wallpaperPath').textContent;
    let startDir = '/';
    if (cur && cur.startsWith(SD_MOUNT + '/')) {
        const rel = cur.slice(SD_MOUNT.length);     // "/wallpapers/10.bin"
        startDir = rel.replace(/\/[^/]+$/, '') || '/';
    }
    browseWallpaper(startDir);
}

function browseWallpaper(path) {
    fetch('/api/sd/list?path=' + encodeURIComponent(path))
        .then(r => r.json())
        .then(renderWallpaperBrowser)
        .catch(() => { document.getElementById('wallpaperBrowser').textContent = 'SD card unavailable'; });
}

function renderWallpaperBrowser(d) {
    const box = document.getElementById('wallpaperBrowser');
    box.innerHTML = '';
    const path = d.path || '/';

    const head = document.createElement('div');
    head.style.cssText = 'font-family:monospace;font-size:12px;margin-bottom:4px;opacity:.8';
    head.textContent = path;
    box.appendChild(head);

    const list = document.createElement('div');
    list.style.cssText = 'max-height:200px;overflow:auto;border:1px solid var(--border,#333);border-radius:6px';
    const addRow = (label, fn) => {
        const r = document.createElement('div');
        r.textContent = label;
        r.style.cssText = 'padding:6px 10px;cursor:pointer';
        r.onmouseenter = () => r.style.background = 'rgba(255,255,255,.06)';
        r.onmouseleave = () => r.style.background = '';
        r.onclick = fn;
        list.appendChild(r);
    };

    if (path !== '/' && path !== '') {
        const parent = path.replace(/\/[^/]+\/?$/, '') || '/';
        addRow('📁 ..', () => browseWallpaper(parent));
    }
    (d.entries || []).forEach(e => {
        const full = (path.endsWith('/') ? path : path + '/') + e.name;
        if (e.dir) { addRow('📁 ' + e.name, () => browseWallpaper(full)); return; }
        if (!e.name.toLowerCase().endsWith('.bin')) return;

        // File row: name selects the wallpaper, 👁 opens a preview (lvbin.js).
        const row = document.createElement('div');
        row.style.cssText = 'padding:6px 10px;cursor:pointer;display:flex;align-items:center;gap:8px';
        row.onmouseenter = () => row.style.background = 'rgba(255,255,255,.06)';
        row.onmouseleave = () => row.style.background = '';
        const name = document.createElement('span');
        name.textContent = '🖼️ ' + e.name;
        name.style.flex = '1';
        name.onclick = () => selectWallpaper(full);
        const eye = document.createElement('button');
        eye.type = 'button';
        eye.className = 'btn-secondary';
        eye.textContent = '👁';
        eye.title = 'Preview';
        eye.style.cssText = 'padding:2px 8px';
        eye.onclick = (ev) => { ev.stopPropagation(); LvBin.openPreview(full); };
        row.append(name, eye);
        list.appendChild(row);
    });
    box.appendChild(list);
}

function selectWallpaper(relPath) {
    const fullPath = SD_MOUNT + relPath;   // fopen needs the mount-point prefix
    document.getElementById('wallpaperPath').textContent = fullPath;
    document.getElementById('wallpaperBrowser').style.display = 'none';
    fetch('/api/settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ display: { wallpaper_on: true, wallpaper_path: fullPath } })
    }).catch(console.error);
}

// ─────────────────────────────────────────────────────────────────────────────
// Boot-info overlay on/off (version + IP on splash, STA only)
// ─────────────────────────────────────────────────────────────────────────────
function setShowBootInfo(on) {
    document.getElementById('settingsBtnBootInfoOn') ?.classList.toggle('active', on);
    document.getElementById('settingsBtnBootInfoOff')?.classList.toggle('active', !on);
    fetch('/api/settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ display: { show_boot_info: on } })
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
// Night dimming schedule (display)
// ─────────────────────────────────────────────────────────────────────────────
function setDimScheduleEnabled(t) {
    document.getElementById('dimSchedOn') ?.classList.toggle('active', t);
    document.getElementById('dimSchedOff')?.classList.toggle('active', !t);
    const panel = document.getElementById('dim_sched_panel');
    if (panel) panel.style.opacity = t ? '1' : '0.5';
}

function parseHHMM(s) {
    const m = /^(\d{1,2}):(\d{2})$/.exec(s || '');
    if (!m) return null;
    const h = parseInt(m[1], 10), mi = parseInt(m[2], 10);
    if (h < 0 || h > 23 || mi < 0 || mi > 59) return null;
    return { h, m: mi };
}

function fmtHHMM(h, m) {
    return String(h).padStart(2, '0') + ':' + String(m).padStart(2, '0');
}

async function saveDimSchedule() {
    const btn = document.getElementById('dim_sched_save_btn');
    if (btn) btn.disabled = true;

    const dim    = parseHHMM(document.getElementById('dim_time').value);
    const bright = parseHHMM(document.getElementById('bright_time').value);
    if (!dim || !bright) {
        showDimSchedStatus('❌ Invalid time', 'error');
        if (btn) btn.disabled = false;
        return;
    }
    const br = parseInt(document.getElementById('dim_brightness_slider').value, 10);
    const enabled = !!document.getElementById('dimSchedOn')?.classList.contains('active');
    const radioOff = !!document.getElementById('night_radio_off')?.checked;
    const radioOn  = !!document.getElementById('night_radio_on')?.checked;
    const station  = parseInt(document.getElementById('night_station')?.value, 10) || 0;
    const volume   = parseInt(document.getElementById('night_volume_slider')?.value, 10);

    try {
        const r = await fetch('/api/settings', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ display: { dim_schedule: {
                enabled,
                dim_hour: dim.h, dim_minute: dim.m,
                dim_brightness: br,
                bright_hour: bright.h, bright_minute: bright.m,
                radio_off: radioOff,
                radio_on: radioOn,
                radio_station: station,
                radio_volume: volume,
            }}})
        });
        if (!r.ok) throw new Error('HTTP ' + r.status);
        showDimSchedStatus('✅ Saved', 'ok');
    } catch (e) {
        showDimSchedStatus('❌ ' + e.message, 'error');
    } finally {
        if (btn) btn.disabled = false;
    }
}

function showDimSchedStatus(text, cls) {
    const el = document.getElementById('dim_sched_status');
    if (!el) return;
    el.innerText = text;
    el.className = 'save-status ' + cls;
    setTimeout(() => { el.innerText = ''; el.className = 'save-status'; }, 3000);
}

// Populate the night-mode wake station picker from the playlist.
async function loadNightStations(selected) {
    const sel = document.getElementById('night_station');
    if (!sel) return;
    let stations = [];
    try {
        const res = await fetch('/api/playlist', { cache: 'no-store' });
        stations = await res.json();
    } catch (e) { stations = []; }
    sel.innerHTML = stations.length
        ? stations.map((s, i) => `<option value="${i}">${i}: ${s.name}</option>`).join('')
        : '<option value="0" disabled>Playlist empty — add stations first</option>';
    if (selected !== undefined && stations.length) sel.value = String(selected);
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
// Audio — exclusive source auto-switch (Radio ⇄ Bluetooth)
// ─────────────────────────────────────────────────────────────────────────────
function setBtAutoSwitch(on) {
    document.getElementById('settingsBtnAutoSwitchOn') ?.classList.toggle('active', on);
    document.getElementById('settingsBtnAutoSwitchOff')?.classList.toggle('active', !on);
    fetch('/api/settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ bluetooth: { auto_switch: on } })
    }).catch(console.error);
}

// ─────────────────────────────────────────────────────────────────────────────
// Resume playback on boot (playlist)
// ─────────────────────────────────────────────────────────────────────────────
function setResumeOnBoot(on) {
    document.getElementById('settingsBtnResumeOn') ?.classList.toggle('active', on);
    document.getElementById('settingsBtnResumeOff')?.classList.toggle('active', !on);
    fetch('/api/settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ playlist: { resume_on_boot: on } })
    }).catch(console.error);
}

// ─────────────────────────────────────────────────────────────────────────────
// Screensaver tab — UI-only handlers (commit via saveScreensaverTab())
// ─────────────────────────────────────────────────────────────────────────────
function setDashboardNotifyEnabled(t) {
    document.getElementById('dashNotifyOn') ?.classList.toggle('active', t);
    document.getElementById('dashNotifyOff')?.classList.toggle('active', !t);
    document.getElementById('dash_notify_panel').style.display = t ? '' : 'none';
}

function onScreensaverStyleChange() {
    const style = document.getElementById('scrs_id').value;
    document.getElementById('dashboard_panel').style.display = style === 'dashboard' ? '' : 'none';
    const pp = document.getElementById('photo_panel');
    if (pp) pp.style.display = style === 'photo' ? '' : 'none';
}

function onDashboardValueTypeChange() {
    const t = document.getElementById('dash_value_type').value;
    document.getElementById('dash_notify_num').style.display = (t === 'number') ? '' : 'none';
    document.getElementById('dash_notify_str').style.display = (t === 'string') ? '' : 'none';
}

async function saveScreensaverTab() {
    const btn = document.getElementById('scrs_save_btn');
    if (btn) btn.disabled = true;

    const get = id => document.getElementById(id);
    const num = id => {
        const v = parseFloat(get(id)?.value);
        return isNaN(v) ? 0 : v;
    };

    let delay = parseInt(get('scrs_delay')?.value, 10);
    if (isNaN(delay) || delay < 0) delay = 0;

    let pollS = parseInt(get('dash_poll_s')?.value, 10);
    if (isNaN(pollS) || pollS < 5) pollS = 5;

    const enabled = !!get('dashNotifyOn')?.classList.contains('active');

    const payload = {
        scrsaver: {
            delay,
            id: get('scrs_id').value,
            photo: {
                dir:    (get('photo_dir')?.value ?? '').trim(),
                order:  parseInt(get('photo_order')?.value, 10)  || 0,
                hold_s: Math.max(1, parseInt(get('photo_hold')?.value, 10) || 8),
                effect: parseInt(get('photo_effect')?.value, 10) || 0,
                speed:  parseInt(get('photo_speed')?.value, 10)  || 3,
                clock:      get('photo_clock')?.checked ? 1 : 0,
                clock_size: parseInt(get('photo_clock_size')?.value, 10) || 96,
            },
        },
        dashboard: {
            title:            (get('dash_title')?.value     ?? '').trim(),
            url:              (get('dash_url')?.value       ?? '').trim(),
            json_path:        (get('dash_json_path')?.value ?? '').trim(),
            suffix:            get('dash_suffix')?.value    ?? '',
            poll_interval_ms:  pollS * 1000,
            notify: {
                enabled,
                value_type:  get('dash_value_type').value,
                num_low_en:  get('dash_num_low_en').checked,
                num_low:     num('dash_num_low'),
                num_high_en: get('dash_num_high_en').checked,
                num_high:    num('dash_num_high'),
                str_eq_en:   get('dash_str_eq_en').checked,
                str_eq:      get('dash_str_eq').value,
                str_ne_en:   get('dash_str_ne_en').checked,
                str_ne:      get('dash_str_ne').value,
            },
        },
    };

    try {
        const r = await fetch('/api/settings', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload)
        });
        if (!r.ok) throw new Error('HTTP ' + r.status);
        showScrsStatus('✅ Saved', 'ok');
    } catch (e) {
        showScrsStatus('❌ ' + e.message, 'error');
    } finally {
        if (btn) btn.disabled = false;
    }
}

function showScrsStatus(text, cls) {
    const el = document.getElementById('scrs_status');
    if (!el) return;
    el.innerText = text;
    el.className = 'save-status ' + cls;
    setTimeout(() => { el.innerText = ''; el.className = 'save-status'; }, 3000);
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

// ─────────────────────────────────────────────────────────────────────────────
// Device name (mDNS) — saved live, no restart
// ─────────────────────────────────────────────────────────────────────────────
async function saveHostname() {
    const btn = document.getElementById('host_save_btn');
    btn.disabled = true;
    showHostStatus('💾 Saving…', '');

    // The field shows the full address; strip the ".local" suffix — the
    // backend stores the bare hostname.
    const hostname = document.getElementById('device_hostname').value
                         .trim().replace(/\.local\.?$/i, '').trim();

    try {
        const res = await fetch('/api/settings', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ device: { hostname } }),
        });
        const data = await res.json();
        if (!data.ok) throw new Error('Device error');

        // Re-read to show the effective (possibly auto/sanitized) name + ".local".
        const r2 = await fetch('/api/settings', { cache: 'no-store' });
        if (r2.ok) {
            const s = await r2.json();
            if (s.device) {
                const name = s.device.effective || s.device.hostname || 'atlascube';
                setVal('device_hostname', name + '.local');
            }
        }
        showHostStatus('✅ Saved.', 'ok');
    } catch (e) {
        showHostStatus('❌ Error: ' + e.message, 'error');
    } finally {
        btn.disabled = false;
    }
}

let _hostStatusTimer = null;
function showHostStatus(msg, type) {
    const el = document.getElementById('host_status');
    if (!el) return;
    el.innerText = msg;
    el.className = 'save-status' + (type ? ' ' + type : '');
    clearTimeout(_hostStatusTimer);
    if (type === 'ok') _hostStatusTimer = setTimeout(
        () => { el.innerText = ''; el.className = 'save-status'; }, 4000);
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
            const verEl = document.getElementById('ota_version');
            if (verEl) verEl.textContent = state.version || 'unknown';
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
    if (s.device) {
        // Show the full "<name>.local" web address so it can be copied directly.
        const name = s.device.effective || s.device.hostname || 'atlascube';
        setVal('device_hostname', name + '.local');
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

        const wallOn = s.display.wallpaper_on === true;
        const bgGrad = s.display.bg_gradient !== false;   // default on
        document.getElementById('settingsBtnBgWall') ?.classList.toggle('active', wallOn);
        document.getElementById('settingsBtnBgGrad') ?.classList.toggle('active', !wallOn && bgGrad);
        document.getElementById('settingsBtnBgSolid')?.classList.toggle('active', !wallOn && !bgGrad);
        document.getElementById('wallpaperPicker').style.display = wallOn ? '' : 'none';
        const wpEl = document.getElementById('wallpaperPath');
        if (wpEl) wpEl.textContent = s.display.wallpaper_path || '(none)';

        const bootInfo = s.display.show_boot_info !== false;   // default on
        document.getElementById('settingsBtnBootInfoOn') ?.classList.toggle('active', bootInfo);
        document.getElementById('settingsBtnBootInfoOff')?.classList.toggle('active', !bootInfo);

        if (s.display.brightness !== undefined) {
            const b = s.display.brightness;
            document.getElementById('disp_bright_slider').value = b;
            document.getElementById('disp_bright_value').innerText = b;
        }

        const ds = s.display.dim_schedule || {};
        const en = !!ds.enabled;
        document.getElementById('dimSchedOn') ?.classList.toggle('active', en);
        document.getElementById('dimSchedOff')?.classList.toggle('active', !en);
        const panel = document.getElementById('dim_sched_panel');
        if (panel) panel.style.opacity = en ? '1' : '0.5';
        setVal('dim_time',    fmtHHMM(ds.dim_hour    ?? 22, ds.dim_minute    ?? 0));
        setVal('bright_time', fmtHHMM(ds.bright_hour ??  7, ds.bright_minute ?? 0));
        const dbv = ds.dim_brightness ?? 20;
        setVal('dim_brightness_slider', dbv);
        const dbl = document.getElementById('dim_brightness_value');
        if (dbl) dbl.textContent = dbv;

        const cbOff = document.getElementById('night_radio_off');
        if (cbOff) cbOff.checked = !!ds.radio_off;
        const cbOn = document.getElementById('night_radio_on');
        if (cbOn) cbOn.checked = !!ds.radio_on;
        const nv = ds.radio_volume ?? 50;
        setVal('night_volume_slider', nv);
        const nvl = document.getElementById('night_volume_value');
        if (nvl) nvl.textContent = nv;
        loadNightStations(ds.radio_station ?? 0);
    }
    if (s.bluetooth) {
        const t = s.bluetooth.show_screen || false;
        document.getElementById('settingsBtnBTshow') ?.classList.toggle('active', t);
        document.getElementById('settingsBtnBThide')?.classList.toggle('active', !t);

        const as = (s.bluetooth.auto_switch !== false);   // default on
        document.getElementById('settingsBtnAutoSwitchOn') ?.classList.toggle('active', as);
        document.getElementById('settingsBtnAutoSwitchOff')?.classList.toggle('active', !as);
    }
    if (s.audio) {
        // default true — when older backend does not return eq_enabled
        const eq_en = (s.audio.eq_enabled !== false);
        document.getElementById('settingsBtnDspOn') ?.classList.toggle('active', eq_en);
        document.getElementById('settingsBtnDspOff')?.classList.toggle('active', !eq_en);
    }
    if (s.playlist) {
        const rob = !!s.playlist.resume_on_boot;   // default off
        document.getElementById('settingsBtnResumeOn') ?.classList.toggle('active', rob);
        document.getElementById('settingsBtnResumeOff')?.classList.toggle('active', !rob);
    }
    if (s.scrsaver) {
        setVal('scrs_delay', s.scrsaver.delay ?? 60);
        const sel = document.getElementById('scrs_id');
        if (sel) sel.value = s.scrsaver.id || 'clockhands';
        const ph = s.scrsaver.photo || {};
        setVal('photo_dir',    ph.dir ?? '/sdcard/slides');
        setVal('photo_order',  String(ph.order  ?? 1));
        setVal('photo_effect', String(ph.effect ?? 4));
        setVal('photo_hold',   ph.hold_s ?? 8);
        setVal('photo_speed',  ph.speed  ?? 3);
        const psv = document.getElementById('photo_speed_value');
        if (psv) psv.textContent = String(ph.speed ?? 3);
        const pcb = document.getElementById('photo_clock');
        if (pcb) pcb.checked = !!(ph.clock ?? 1);
        setVal('photo_clock_size', String(ph.clock_size ?? 96));
        onScreensaverStyleChange();
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
// MQTT — broker config only; widget editor lives at /mqtt.html
// State is fetched/saved via /api/mqtt (not /api/settings).
// ─────────────────────────────────────────────────────────────────────────────
function setMqttEnabled(t) {
    document.getElementById('mqttBtnOn') ?.classList.toggle('active', t);
    document.getElementById('mqttBtnOff')?.classList.toggle('active', !t);
}

async function loadMqtt() {
    try {
        const r = await fetch('/api/mqtt', { cache: 'no-store' });
        if (!r.ok) throw new Error('HTTP ' + r.status);
        const c = await r.json();
        const en = !!c.enabled;
        document.getElementById('mqttBtnOn') ?.classList.toggle('active', en);
        document.getElementById('mqttBtnOff')?.classList.toggle('active', !en);
        setVal('mqtt_host',       c.host       || '');
        setVal('mqtt_port',       c.port       || 1883);
        setVal('mqtt_username',   c.username   || '');
        setVal('mqtt_client_id',  c.client_id  || '');
        setVal('mqtt_base_topic', c.base_topic || '');
    } catch (e) {
        console.error('loadMqtt', e);
    }
}

async function saveMqtt() {
    const btn = document.getElementById('mqtt_save_btn');
    btn.disabled = true;

    const enabled = document.getElementById('mqttBtnOn')?.classList.contains('active') || false;
    const get = id => document.getElementById(id)?.value ?? '';

    let port = parseInt(get('mqtt_port'), 10);
    if (isNaN(port) || port < 1 || port > 65535) port = 1883;

    const payload = {
        enabled,
        host:       get('mqtt_host').trim(),
        port,
        username:   get('mqtt_username').trim(),
        password:   get('mqtt_password'),       // empty → keep old
        client_id:  get('mqtt_client_id').trim(),
        base_topic: get('mqtt_base_topic').trim(),
        // widgets omitted — editor at /mqtt.html owns that array
    };

    try {
        const r = await fetch('/api/mqtt', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload),
        });
        if (!r.ok) throw new Error('HTTP ' + r.status);
        showMqttStatus('✅ Saved. Client reconnecting…', 'ok');
        document.getElementById('mqtt_password').value = '';
    } catch (e) {
        showMqttStatus('❌ ' + e.message, 'error');
    } finally {
        btn.disabled = false;
    }
}

let _mqttStatusTimer = null;
function showMqttStatus(msg, type) {
    const el = document.getElementById('mqtt_status');
    if (!el) return;
    el.innerText = msg;
    el.className = 'save-status' + (type ? ' ' + type : '');
    clearTimeout(_mqttStatusTimer);
    if (type === 'ok') _mqttStatusTimer = setTimeout(
        () => { el.innerText = ''; el.className = 'save-status'; }, 4000);
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
    ['bg_grad_top',    'Gradient top',        'Top colour of the background gradient'],
    ['bg_grad_bottom', 'Gradient bottom',     'Bottom colour of the background gradient'],
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
// Firmware update (OTA) — streams the .bin to POST /api/ota with a progress bar
// ─────────────────────────────────────────────────────────────────────────────
function showOtaStatus(msg, type) {
    const el = document.getElementById('ota_status');
    if (!el) return;
    el.innerText = msg;
    el.className = 'save-status' + (type ? ' ' + type : '');
}

function uploadFirmware() {
    const fileEl = document.getElementById('ota_file');
    const btn    = document.getElementById('ota_btn');
    const wrap   = document.getElementById('ota_progress_wrap');
    const bar    = document.getElementById('ota_progress');
    const pct    = document.getElementById('ota_progress_pct');

    const file = fileEl.files[0];
    if (!file) { showOtaStatus('❌ Pick a .bin file first', 'error'); return; }
    if (!file.name.endsWith('.bin')) {
        showOtaStatus('❌ Expected a .bin firmware image', 'error');
        return;
    }
    if (!confirm('Flash "' + file.name + '" (' +
                 (file.size / 1048576).toFixed(2) + ' MB)? ' +
                 'The device will reboot when done.')) {
        return;
    }

    btn.disabled = true;
    wrap.classList.remove('hidden');
    showOtaStatus('Uploading…', '');

    const xhr = new XMLHttpRequest();
    xhr.open('POST', '/api/ota', true);
    xhr.setRequestHeader('Content-Type', 'application/octet-stream');

    xhr.upload.onprogress = (e) => {
        if (!e.lengthComputable) return;
        const p = Math.round((e.loaded / e.total) * 100);
        bar.value = p;
        pct.textContent = p + '%';
    };

    xhr.onload = () => {
        if (xhr.status === 200) {
            bar.value = 100; pct.textContent = '100%';
            showOtaStatus('✅ Installed. Device is rebooting into the new firmware…', 'ok');
        } else {
            // The device returns a plain-text error body for 4xx/5xx
            showOtaStatus('❌ ' + (xhr.responseText || ('HTTP ' + xhr.status)), 'error');
            btn.disabled = false;
        }
    };
    xhr.onerror = () => {
        // A connection drop right after the response is expected (device reboots);
        // only treat it as an error if no progress reached completion.
        if (bar.value >= 100) {
            showOtaStatus('✅ Uploaded. Device is rebooting…', 'ok');
        } else {
            showOtaStatus('❌ Upload failed (connection lost)', 'error');
            btn.disabled = false;
        }
    };

    xhr.send(file);
}

// ─────────────────────────────────────────────────────────────────────────────
// Init
// ─────────────────────────────────────────────────────────────────────────────
initTabs();
loadSettings();
loadColors();
loadMqtt();