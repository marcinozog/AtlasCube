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
const SUB_STORAGE_KEY = 'atlascube.settings.displaySub';
const DEFAULT_TAB     = 'display';
const DEFAULT_SUB     = 'panel';

function getKnownTabs() {
    return Array.from(document.querySelectorAll('#settingsTabNav .tab-btn'))
                .map(b => b.dataset.tab);
}

function getKnownSubs() {
    return Array.from(document.querySelectorAll('#displaySubNav .tab-btn'))
                .map(b => b.dataset.sub);
}

// Persist in URL fragment so deep links work without hitting the server
// with a query string the file handler can't resolve.
function updateTabHash(tab, sub) {
    const newHash = '#tab=' + tab + (sub ? '&sub=' + sub : '');
    if (window.location.hash !== newHash) {
        window.history.replaceState(null, '', window.location.pathname + newHash);
    }
}

// Sub-tabs inside the Display tab (Panel / Theme).
function selectDisplaySub(name) {
    if (!getKnownSubs().includes(name)) name = DEFAULT_SUB;

    for (const btn  of document.querySelectorAll('#displaySubNav .tab-btn')) {
        btn.classList.toggle('active', btn.dataset.sub === name);
    }
    for (const pane of document.querySelectorAll('#tab-display .sub-pane')) {
        pane.classList.toggle('active', pane.dataset.sub === name);
    }
    try { localStorage.setItem(SUB_STORAGE_KEY, name); } catch (e) { /* private mode */ }
    updateTabHash('display', name);
}

function selectTab(name, sub) {
    // Legacy deep links: Theme used to be its own tab.
    if (name === 'colors') { name = 'display'; sub = sub || 'theme'; }

    const known = getKnownTabs();
    if (!known.includes(name)) name = DEFAULT_TAB;

    for (const btn  of document.querySelectorAll('#settingsTabNav .tab-btn')) {
        btn.classList.toggle('active', btn.dataset.tab === name);
    }
    for (const pane of document.querySelectorAll('.tab-pane')) {
        pane.classList.toggle('active', pane.dataset.tab === name);
    }
    try { localStorage.setItem(TAB_STORAGE_KEY, name); } catch (e) { /* private mode */ }

    if (name === 'display') {
        if (!sub) {
            try { sub = localStorage.getItem(SUB_STORAGE_KEY); } catch (e) { /* ignore */ }
        }
        selectDisplaySub(sub || DEFAULT_SUB);   // also updates the hash
    } else {
        updateTabHash(name);
    }
}

function tabFromHash() {
    const h = window.location.hash || '';
    const t = /(?:^|[#&])tab=([^&]+)/.exec(h);
    const s = /(?:^|[#&])sub=([^&]+)/.exec(h);
    return { tab: t ? decodeURIComponent(t[1]) : null,
             sub: s ? decodeURIComponent(s[1]) : null };
}

function initTabs() {
    const known = getKnownTabs();
    // 'colors' is not a real tab any more but selectTab() maps it.
    const isValid  = t => known.includes(t) || t === 'colors';
    const fromHash = tabFromHash();
    let   savedTab = null;
    try { savedTab = localStorage.getItem(TAB_STORAGE_KEY); } catch (e) { /* ignore */ }

    const initial = (fromHash.tab && isValid(fromHash.tab)) ? fromHash.tab
                  : (savedTab    && isValid(savedTab))      ? savedTab
                  : DEFAULT_TAB;
    selectTab(initial, fromHash.sub);

    // React to manual hash edits / back-forward navigation.
    window.addEventListener('hashchange', () => {
        const t = tabFromHash();
        if (t.tab) selectTab(t.tab, t.sub);
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
    const wpEl = document.getElementById('wallpaperPath');
    const cur  = wpEl ? wpEl.textContent : '';
    const realPath = (cur && cur !== '(none)') ? cur : '';
    const body = wall ? { display: { wallpaper_on: true, wallpaper_path: realPath } }
                      : { display: { wallpaper_on: false, bg_gradient: grad } };
    fetch('/api/settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body)
    }).catch(console.error);
}

// Generic SD .bin picker (reuses /api/sd/list), shared by the wallpaper and the
// splash-logo controls. Renders into `box`; `onSelect(relPath)` fires when a
// file is clicked, 👁 previews it (lvbin.js).
function postDisplay(patch) {
    fetch('/api/settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ display: patch })
    }).catch(console.error);
}

function setWallpaperDim(v) {
    postDisplay({ wallpaper_dim: parseInt(v, 10) || 0 });
}

// Folder of the currently selected file (mount-relative), or '/' if none.
function sdDirOf(fullText) {
    if (fullText && fullText.startsWith(SD_MOUNT + '/')) {
        const rel = fullText.slice(SD_MOUNT.length);   // "/wallpapers/10.bin"
        return rel.replace(/\/[^/]+$/, '') || '/';
    }
    return '/';
}

function browseBin(box, path, onSelect) {
    fetch('/api/sd/list?path=' + encodeURIComponent(path))
        .then(r => r.json())
        .then(d => renderBinBrowser(box, d, onSelect))
        .catch(() => { box.textContent = 'SD card unavailable'; });
}

function renderBinBrowser(box, d, onSelect) {
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
        addRow('📁 ..', () => browseBin(box, parent, onSelect));
    }
    // Directories first, then files, each group alphabetically.
    const entries = (d.entries || []).slice().sort((a, b) =>
        (!!b.dir - !!a.dir) ||
        a.name.localeCompare(b.name, undefined, { numeric: true, sensitivity: 'base' }));
    entries.forEach(e => {
        const full = (path.endsWith('/') ? path : path + '/') + e.name;
        if (e.dir) { addRow('📁 ' + e.name, () => browseBin(box, full, onSelect)); return; }
        if (!e.name.toLowerCase().endsWith('.bin')) return;

        // File row: name picks the file, 👁 opens a preview (lvbin.js).
        const row = document.createElement('div');
        row.style.cssText = 'padding:6px 10px;cursor:pointer;display:flex;align-items:center;gap:8px';
        row.onmouseenter = () => row.style.background = 'rgba(255,255,255,.06)';
        row.onmouseleave = () => row.style.background = '';
        const name = document.createElement('span');
        name.textContent = '🖼️ ' + e.name;
        name.style.flex = '1';
        name.onclick = () => onSelect(full);
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

// ── Wallpaper picker ──────────────────────────────────────────────────────────
function openWallpaperBrowser() {
    const box = document.getElementById('wallpaperBrowser');
    box.style.display = '';
    browseBin(box, sdDirOf(document.getElementById('wallpaperPath').textContent),
              selectWallpaper);
}

function selectWallpaper(relPath) {
    const fullPath = SD_MOUNT + relPath;   // fopen needs the mount-point prefix
    document.getElementById('wallpaperPath').textContent = fullPath;
    document.getElementById('wallpaperBrowser').style.display = 'none';
    postDisplay({ wallpaper_on: true, wallpaper_path: fullPath });
}

function wallpaperUploadStatus(message, kind) {
    const el = document.getElementById('wallpaperUploadStatus');
    el.textContent = message;
    el.className = 'save-status' + (kind ? ' ' + kind : '');
}

async function wallpaperPanelSize() {
    if (netWpPanelW > 0 && netWpPanelH > 0) return { w: netWpPanelW, h: netWpPanelH };
    const r = await fetch('/api/ui/profile/meta', { cache: 'no-store' });
    if (!r.ok) throw new Error('panel info HTTP ' + r.status);
    const m = await r.json();
    const w = Number(m.screen_w), h = Number(m.screen_h);
    if (!Number.isInteger(w) || !Number.isInteger(h) || w < 1 || h < 1)
        throw new Error('device returned invalid panel dimensions');
    netWpPanelW = w; netWpPanelH = h;
    return { w, h };
}

async function uploadWallpaperImage(input) {
    const file = input.files && input.files[0];
    input.value = '';
    if (!file) return;

    const label = document.getElementById('wallpaperUploadBtn');
    label.style.pointerEvents = 'none';
    label.style.opacity = '.6';
    try {
        const { w, h } = await wallpaperPanelSize();
        const relPath = await LvBin.uploadImage(file, '/wallpapers', w, h,
            msg => wallpaperUploadStatus(msg, ''));
        selectWallpaper(relPath);
        wallpaperUploadStatus('✓ Uploaded and applied', 'ok');
    } catch (e) {
        wallpaperUploadStatus('✕ ' + e.message, 'error');
    } finally {
        label.style.pointerEvents = '';
        label.style.opacity = '';
    }
}

// ── Internet wallpaper (test) ────────────────────────────────────────────────
// POST the URL, then poll /api/wallpaper/status until the fetch task settles.
// On success the device repaints itself (UI_EVT_BG_CHANGED) — nothing else to do.
// The preset list lives in settings.html: plain bookmarks the user can pick
// from — the device fetches directly from the service (nothing is re-hosted),
// and the firmware only ever sees the final URL.
let netWpTimer = null;
let netWpPanelW = 0, netWpPanelH = 0;   // panel size for the {w}/{h} preview

// Panel dimensions come from the layout-editor meta endpoint; the preview
// line stays empty until they arrive.
function loadNetWpMeta() {
    fetch('/api/ui/profile/meta').then(r => r.json()).then(m => {
        netWpPanelW = m.screen_w || 0;
        netWpPanelH = m.screen_h || 0;
        updateNetWpResolved();
    }).catch(console.error);
}

// Show what the device will actually request: the URL with {w}/{h} expanded,
// or a note that a placeholder-less URL is fetched as-is and scaled on-device.
function updateNetWpResolved() {
    const el  = document.getElementById('netWpResolved');
    const url = document.getElementById('netWpUrl').value.trim();
    if (!url || !netWpPanelW) { el.textContent = ''; return; }
    if (url.includes('{w}') || url.includes('{h}')) {
        el.textContent = '→ ' + url.replaceAll('{w}', netWpPanelW)
                                   .replaceAll('{h}', netWpPanelH);
    } else {
        el.textContent = '→ fetched as-is, scaled to ' + netWpPanelW + '×' +
                         netWpPanelH + ' on the device';
    }
}

function netWpPresetChanged() {
    const v = document.getElementById('netWpPreset').value;
    if (v) document.getElementById('netWpUrl').value = v;   // '' = Custom: keep what's typed
    updateNetWpResolved();
}

// The URL field is the single source of truth; the select just mirrors it
// (matching preset, or "Custom URL…" otherwise — same behaviour as the
// Android app). Fired on URL edits and after settings load.
function syncNetWpPreset() {
    const url = document.getElementById('netWpUrl').value.trim();
    const sel = document.getElementById('netWpPreset');
    const match = Array.from(sel.options).find(o => o.value && o.value === url);
    sel.value = match ? match.value : '';
    updateNetWpResolved();
}

// Persist URL + auto-refresh mode/time in one patch; the firmware re-arms its
// scheduler on this POST. Fired by the mode select and the time picker.
function saveNetWpSchedule() {
    const mode = parseInt(document.getElementById('netWpMode').value, 10) || 0;
    const timeEl = document.getElementById('netWpTime');
    timeEl.style.display = (mode === 2) ? '' : 'none';
    const [h, m] = (timeEl.value || '04:00').split(':').map(Number);
    postDisplay({
        wallpaper_url:        document.getElementById('netWpUrl').value.trim(),
        wallpaper_fetch_mode: mode,
        wallpaper_fetch_hour: h,
        wallpaper_fetch_min:  m
    });
}

function fetchNetWallpaper() {
    const url = document.getElementById('netWpUrl').value.trim();
    const st  = document.getElementById('netWpStatus');
    if (!url) return;
    postDisplay({ wallpaper_url: url });   // keep the scheduled URL in sync
    st.textContent = 'starting…';
    fetch('/api/wallpaper/fetch', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ url })
    }).then(r => r.json()).then(j => {
        if (j.result !== 'started') { st.textContent = j.result; return; }
        pollNetWallpaper();
    }).catch(() => { st.textContent = 'request failed'; });
}

// Persist the fetched wallpaper on the SD card (/sdcard/wallpapers/saved/…);
// the file then shows up in the regular SD-wallpaper picker. Background
// settings are untouched — this only collects the image.
function saveNetWallpaper() {
    const st = document.getElementById('netWpStatus');
    st.textContent = 'saving…';
    fetch('/api/wallpaper/save', { method: 'POST' })
        .then(r => r.json())
        .then(j => {
            st.textContent = (j.result === 'ok') ? ('saved: ' + j.path)
                                                 : (j.error || 'save failed');
        })
        .catch(() => { st.textContent = 'request failed'; });
}

function pollNetWallpaper() {
    clearTimeout(netWpTimer);
    fetch('/api/wallpaper/status').then(r => r.json()).then(j => {
        document.getElementById('netWpStatus').textContent = j.status;
        if (j.status === 'busy') netWpTimer = setTimeout(pollNetWallpaper, 1000);
    }).catch(console.error);
}

// ── Splash-logo picker ──────────────────────────────────────────────────────────
function openLogoBrowser() {
    const box = document.getElementById('logoBrowser');
    box.style.display = '';
    browseBin(box, sdDirOf(document.getElementById('logoPath').textContent), selectLogo);
}

function selectLogo(relPath) {
    const fullPath = SD_MOUNT + relPath;
    document.getElementById('logoPath').textContent = fullPath;
    document.getElementById('logoBrowser').style.display = 'none';
    postDisplay({ logo_path: fullPath });
}

function resetLogo() {
    document.getElementById('logoPath').textContent = '(built-in)';
    document.getElementById('logoBrowser').style.display = 'none';
    postDisplay({ logo_path: '' });
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
// Auto-update on-screen prompt (the boot check always runs regardless)
// ─────────────────────────────────────────────────────────────────────────────
function setUpdateEnable(on) {
    document.getElementById('settingsBtnUpdateOn') ?.classList.toggle('active', on);
    document.getElementById('settingsBtnUpdateOff')?.classList.toggle('active', !on);
    fetch('/api/settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ update: { enable: on } })
    }).catch(console.error);
}

// ─────────────────────────────────────────────────────────────────────────────
// Flip display 180° (applied live — no restart)
// ─────────────────────────────────────────────────────────────────────────────
function setFlip(on) {
    document.getElementById('settingsBtnFlipOn') ?.classList.toggle('active', on);
    document.getElementById('settingsBtnFlipOff')?.classList.toggle('active', !on);
    fetch('/api/settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ display: { flip: on } })
    }).catch(console.error);
}

// ─────────────────────────────────────────────────────────────────────────────
// Invert panel colours (applied live — no restart)
// ─────────────────────────────────────────────────────────────────────────────
function setInvert(on) {
    document.getElementById('settingsBtnInvertOn') ?.classList.toggle('active', on);
    document.getElementById('settingsBtnInvertOff')?.classList.toggle('active', !on);
    fetch('/api/settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ display: { invert: on } })
    }).catch(console.error);
}

// ─────────────────────────────────────────────────────────────────────────────
// Time format: 12-hour AM/PM vs 24-hour (applied live — no restart)
// ─────────────────────────────────────────────────────────────────────────────
function setTimeAmpm(on) {
    document.getElementById('settingsBtnTime12')?.classList.toggle('active', on);
    document.getElementById('settingsBtnTime24')?.classList.toggle('active', !on);
    fetch('/api/settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ display: { time_ampm: on } })
    }).catch(console.error);
}

// ─────────────────────────────────────────────────────────────────────────────
// Date format: MM/DD/YYYY vs YYYY-MM-DD (applied live — no restart)
// ─────────────────────────────────────────────────────────────────────────────
function setDateMdy(on) {
    document.getElementById('settingsBtnDateMdy')?.classList.toggle('active', on);
    document.getElementById('settingsBtnDateYmd')?.classList.toggle('active', !on);
    fetch('/api/settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ display: { date_mdy: on } })
    }).catch(console.error);
}

// ─────────────────────────────────────────────────────────────────────────────
// FPS overlay (applied live — no restart)
// ─────────────────────────────────────────────────────────────────────────────
function setShowFps(on) {
    document.getElementById('settingsBtnFpsOn') ?.classList.toggle('active', on);
    document.getElementById('settingsBtnFpsOff')?.classList.toggle('active', !on);
    fetch('/api/settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ display: { show_fps: on } })
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
// SD player screen (display)
// ─────────────────────────────────────────────────────────────────────────────
function setDeviceSDScreen(t) {
    document.getElementById('settingsBtnSDshow')?.classList.toggle('active', t);
    document.getElementById('settingsBtnSDhide')?.classList.toggle('active', !t);
    fetch('/api/settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ display: { sd_show_screen: t } })
    }).catch(console.error);
}

// ─────────────────────────────────────────────────────────────────────────────
// Radio screen (display)
// ─────────────────────────────────────────────────────────────────────────────
function setDeviceRadioScreen(t) {
    document.getElementById('settingsBtnRadioShow')?.classList.toggle('active', t);
    document.getElementById('settingsBtnRadioHide')?.classList.toggle('active', !t);
    fetch('/api/settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ display: { radio_show_screen: t } })
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
    const dp = document.getElementById('dim_panel');
    if (dp) dp.style.display = style === 'dim' ? '' : 'none';
    const mp = document.getElementById('mqtt_scrs_panel');
    if (mp) mp.style.display = style === 'mqtt' ? '' : 'none';
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
            dim_level: parseInt(get('scrs_dim_level')?.value, 10) || 0,
            block_when_playing: get('scrs_block_play')?.checked ? 1 : 0,
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
            if (verEl) {
                // Append the web-UI hash this firmware expects, so the same hash
                // shows here and in the "Web UI files" panel below — a match means
                // the web UI is in sync, a mismatch means an app-only OTA left it stale.
                const h = (state.www_expected || '').split(/\s+/)[0].slice(0, 8);
                verEl.textContent = (state.version || 'unknown') + (h ? ' · ' + h : '');
            }
            showWwwState(state);
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
        const isWall = wallOn;
        const bgGrad = s.display.bg_gradient !== false;   // default on
        document.getElementById('settingsBtnBgWall') ?.classList.toggle('active', isWall);
        document.getElementById('settingsBtnBgGrad') ?.classList.toggle('active', !wallOn && bgGrad);
        document.getElementById('settingsBtnBgSolid')?.classList.toggle('active', !wallOn && !bgGrad);
        document.getElementById('wallpaperPicker').style.display = isWall ? '' : 'none';
        const wpEl = document.getElementById('wallpaperPath');
        if (wpEl) wpEl.textContent = s.display.wallpaper_path || '(none)';
        // Slider shows brightness (100 - dim): right = brighter, like the
        // panel Brightness slider.
        const wpBright = 100 - (s.display.wallpaper_dim || 0);
        document.getElementById('wp_dim_slider').value = wpBright;
        document.getElementById('wp_dim_value').textContent = wpBright + '%';

        if (s.display.wallpaper_url)
            document.getElementById('netWpUrl').value = s.display.wallpaper_url;
        syncNetWpPreset();
        const wfMode = s.display.wallpaper_fetch_mode || 0;
        document.getElementById('netWpMode').value = String(wfMode);
        const wfTime = document.getElementById('netWpTime');
        wfTime.style.display = (wfMode === 2) ? '' : 'none';
        const p2 = n => String(n === undefined ? 0 : n).padStart(2, '0');
        wfTime.value = p2(s.display.wallpaper_fetch_hour === undefined ? 4
                          : s.display.wallpaper_fetch_hour) + ':' +
                       p2(s.display.wallpaper_fetch_min);

        const flip = s.display.flip === true;
        document.getElementById('settingsBtnFlipOn') ?.classList.toggle('active', flip);
        document.getElementById('settingsBtnFlipOff')?.classList.toggle('active', !flip);

        const invert = s.display.invert === true;
        document.getElementById('settingsBtnInvertOn') ?.classList.toggle('active', invert);
        document.getElementById('settingsBtnInvertOff')?.classList.toggle('active', !invert);

        const ampm = s.display.time_ampm === true;   // default 24-hour
        document.getElementById('settingsBtnTime12')?.classList.toggle('active', ampm);
        document.getElementById('settingsBtnTime24')?.classList.toggle('active', !ampm);

        const mdy = s.display.date_mdy === true;     // default YYYY-MM-DD
        document.getElementById('settingsBtnDateMdy')?.classList.toggle('active', mdy);
        document.getElementById('settingsBtnDateYmd')?.classList.toggle('active', !mdy);

        const showFps = s.display.show_fps === true;   // default off
        document.getElementById('settingsBtnFpsOn') ?.classList.toggle('active', showFps);
        document.getElementById('settingsBtnFpsOff')?.classList.toggle('active', !showFps);

        const bootInfo = s.display.show_boot_info !== false;   // default on
        document.getElementById('settingsBtnBootInfoOn') ?.classList.toggle('active', bootInfo);
        document.getElementById('settingsBtnBootInfoOff')?.classList.toggle('active', !bootInfo);

        const updEnable = s.update?.enable !== false;   // default on
        document.getElementById('settingsBtnUpdateOn') ?.classList.toggle('active', updEnable);
        document.getElementById('settingsBtnUpdateOff')?.classList.toggle('active', !updEnable);

        const sdScr = s.display.sd_show_screen !== false;   // default on
        document.getElementById('settingsBtnSDshow')?.classList.toggle('active', sdScr);
        document.getElementById('settingsBtnSDhide')?.classList.toggle('active', !sdScr);

        const radioScr = s.display.radio_show_screen !== false;   // default on
        document.getElementById('settingsBtnRadioShow')?.classList.toggle('active', radioScr);
        document.getElementById('settingsBtnRadioHide')?.classList.toggle('active', !radioScr);

        const logoEl = document.getElementById('logoPath');
        if (logoEl) logoEl.textContent = s.display.logo_path || '(built-in)';

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
        const bwp = document.getElementById('scrs_block_play');
        if (bwp) bwp.checked = !!(s.scrsaver.block_when_playing ?? 0);
        const dlv = s.scrsaver.dim_level ?? 20;
        setVal('scrs_dim_level', dlv);
        const dlvLbl = document.getElementById('scrs_dim_level_value');
        if (dlvLbl) dlvLbl.textContent = String(dlv);
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

// ─────────────────────────────────────────────────────────────────────────────
// Weather — home-screen widget config, fetched/saved via /api/weather.
// ─────────────────────────────────────────────────────────────────────────────
function setWeatherEnabled(t) {
    document.getElementById('weatherBtnOn') ?.classList.toggle('active', t);
    document.getElementById('weatherBtnOff')?.classList.toggle('active', !t);
}

function onWeatherProvider() {
    const owm = document.getElementById('weather_provider')?.value === '1';
    const panel = document.getElementById('weather_key_panel');
    if (panel) panel.style.display = owm ? '' : 'none';
}

// Mirrors condition() in weather_widget.c (WMO weather codes).
function weatherCondition(code) {
    if (code === 0) return 'Clear';
    if (code <= 2) return 'Partly cloudy';
    if (code === 3) return 'Cloudy';
    if (code === 45 || code === 48) return 'Fog';
    if (code >= 51 && code <= 67) return 'Rain';
    if (code >= 71 && code <= 77) return 'Snow';
    if (code >= 80 && code <= 82) return 'Showers';
    if (code >= 85 && code <= 86) return 'Snow showers';
    if (code >= 95) return 'Storm';
    return 'Weather';
}

async function loadWeather() {
    try {
        const r = await fetch('/api/weather', { cache: 'no-store' });
        if (!r.ok) throw new Error('HTTP ' + r.status);
        const c = await r.json();
        setWeatherEnabled(!!c.enabled);
        setVal('weather_provider', c.provider || 0);
        setVal('weather_api_key',  c.api_key || '');
        onWeatherProvider();
        setVal('weather_lat',     c.latitude);
        setVal('weather_lon',     c.longitude);
        setVal('weather_refresh', c.refresh_min || 30);
        const now = document.getElementById('weather_now');
        if (now) now.innerText = c.valid
            ? Math.round(c.temperature_c) + '°C, ' + weatherCondition(c.weather_code) +
              ', humidity ' + c.humidity_pct + '%'
            : (c.enabled ? 'No data yet — fetch pending or failed.' : 'Disabled.');
    } catch (e) {
        console.error('loadWeather', e);
    }
}

async function findWeatherLocation() {
    const btn = document.getElementById('weather_location_btn');
    const query = document.getElementById('weather_location_query')?.value.trim();
    if (!query) {
        showWeatherStatus('Enter a city or postal code.', 'error');
        return;
    }

    btn.disabled = true;
    btn.innerText = 'Searching...';
    showWeatherStatus('Searching for location...');

    try {
        const language = (navigator.language || 'en').split('-')[0].toLowerCase();
        const url = 'https://geocoding-api.open-meteo.com/v1/search?name=' +
            encodeURIComponent(query) + '&count=1&language=' + encodeURIComponent(language) + '&format=json';
        const r = await fetch(url, { cache: 'no-store' });
        if (!r.ok) throw new Error('HTTP ' + r.status);
        const data = await r.json();
        const location = data.results?.[0];
        if (!location) {
            showWeatherStatus('No matching location found.', 'error');
            return;
        }

        setVal('weather_lat', Number(location.latitude).toFixed(5));
        setVal('weather_lon', Number(location.longitude).toFixed(5));
        const parts = [location.name, location.admin1, location.country].filter(Boolean);
        showWeatherStatus('Found: ' + parts.join(', ') + '. Save Weather to apply it.', 'ok');
    } catch (e) {
        showWeatherStatus('Could not search for the location: ' + e.message, 'error');
    } finally {
        btn.disabled = false;
        btn.innerText = '\u{1F50D} Find location';
    }
}

async function saveWeather() {
    const btn = document.getElementById('weather_save_btn');
    btn.disabled = true;

    const enabled = document.getElementById('weatherBtnOn')?.classList.contains('active') || false;
    const get = id => document.getElementById(id)?.value ?? '';
    const provider = parseInt(get('weather_provider'), 10) || 0;
    const apiKey = get('weather_api_key').trim();
    const lat = parseFloat(get('weather_lat'));
    const lon = parseFloat(get('weather_lon'));
    let refresh = parseInt(get('weather_refresh'), 10);
    if (isNaN(refresh)) refresh = 30;

    if (enabled && (isNaN(lat) || isNaN(lon))) {
        showWeatherStatus('❌ Latitude and longitude are required.', 'error');
        btn.disabled = false;
        return;
    }
    if (enabled && provider === 1 && !apiKey) {
        showWeatherStatus('❌ OpenWeatherMap needs an API key.', 'error');
        btn.disabled = false;
        return;
    }

    const payload = { enabled, provider, api_key: apiKey, refresh_min: refresh };
    if (!isNaN(lat)) payload.latitude = lat;
    if (!isNaN(lon)) payload.longitude = lon;

    try {
        const r = await fetch('/api/weather', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload),
        });
        if (!r.ok) throw new Error('HTTP ' + r.status);
        showWeatherStatus(enabled ? '✅ Saved. Fetching weather…' : '✅ Saved.', 'ok');
        // The device re-fetches within seconds of a config change — refresh the
        // readout once the request had time to finish (12s HTTP timeout).
        if (enabled) { setTimeout(loadWeather, 5000); setTimeout(loadWeather, 15000); }
        else loadWeather();
    } catch (e) {
        showWeatherStatus('❌ ' + e.message, 'error');
    } finally {
        btn.disabled = false;
    }
}

let _weatherStatusTimer = null;
function showWeatherStatus(msg, type) {
    const el = document.getElementById('weather_status');
    if (!el) return;
    el.innerText = msg;
    el.className = 'save-status' + (type ? ' ' + type : '');
    clearTimeout(_weatherStatusTimer);
    if (type === 'ok') _weatherStatusTimer = setTimeout(
        () => { el.innerText = ''; el.className = 'save-status'; }, 4000);
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
// [key, label, hint, section]
const COLOR_FIELDS = [
    ['bg_primary',     'Screen background',   'Main background of all screens',                 'Background'],
    ['bg_secondary',   'Panels & cards',      'Strips, cards, sliders track',                   'Background'],
    ['bg_grad_top',    'Gradient top',        'Top colour of the background gradient',          'Background'],
    ['bg_grad_bottom', 'Gradient bottom',     'Bottom colour of the background gradient',       'Background'],
    ['text_primary',   'Main text',           'Clock digits, titles, main labels',              'Text'],
    ['text_secondary', 'Secondary text',      'Date, station name in strip, volume %',          'Text'],
    ['text_muted',     'Subtle text',         'Audio info, ICY title, "Syncing…"',              'Text'],
    ['accent',         'Accent / highlight',  'Radio station name, slider fill',                'Accents'],
    ['bt_brand',       'Bluetooth',           'BT icon, "Bluetooth Audio", BT slider',          'Accents'],
    ['status_ok',      'Status OK',           'Connected / Playing (semantic green)',           'Accents'],
    ['vu_bg',          'VU meter background', 'Spectrum/VU meter panel background',              'VU / spectrum'],
    ['vu_bar',         'VU meter bars',       'Spectrum/VU meter bar colour',                   'VU / spectrum'],
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
    let lastSection = null;
    grid.innerHTML = COLOR_FIELDS.map(([key, label, hint, section]) => {
        const v = (pal[key] || '#000000').toUpperCase();
        const header = section && section !== lastSection
            ? `<div class="color-section">${section}</div>` : '';
        lastSection = section;
        return header + `
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

// Toggle the "web UI out of date" warning and show both hashes so a mismatch is
// visible: www_version is what sits on the www partition, www_expected is what
// this firmware was built against. They differ after an app-only OTA.
// www_version.txt is "<hash> <git-describe> <date> <time> UTC". The middle
// git-describe drifts from the firmware version and only confuses, so render
// just the build time + short hash: "2026-06-27 10:18 UTC · 7fcf1a8b".
function fmtWwwBuilt(raw) {
    if (!raw) return 'unknown';
    const p = raw.split(/\s+/);
    if (p.length < 4) return raw;
    const hash = p[0].slice(0, 8);
    const time = p[p.length - 2].slice(0, 5);   // drop seconds
    const date = p[p.length - 3];
    return date + ' ' + time + ' UTC · ' + hash;
}

function showWwwState(state) {
    const verEl = document.getElementById('www_ver');
    if (verEl) verEl.textContent = fmtWwwBuilt(state && state.www_version);
    const warnEl = document.getElementById('www_outdated_warn');
    if (warnEl) warnEl.style.display = state && state.www_outdated ? '' : 'none';
    const box = document.getElementById('www_hashes');
    if (box) {
        box.textContent = state && state.www_outdated
            ? 'On device: ' + (state.www_version || '(none)') +
              '  •  Firmware expects: ' + (state.www_expected || '(unknown)')
            : '';
    }
}

// Re-upload the raw web UI files to the www partition via /api/files (the device
// gzips HTML/CSS/JS). Sequential — the endpoint takes one file per request. After
// the batch, re-read /api/state so the "out of date" warning clears once the new
// www_version.txt has landed.
// PUT a single file, retrying a few times on a dropped connection — uploading
// the whole www set back-to-back can transiently starve the device's internal
// RAM (see radio-https-fragility) and reset one socket; a retry clears it.
async function putWithRetry(file, tries = 3) {
    for (let a = 1; a <= tries; a++) {
        try {
            const r = await fetch('/api/files/' + encodeURIComponent(file.name),
                                  { method: 'PUT', body: file });
            if (r.ok) return true;
        } catch (_) { /* connection reset — fall through to retry */ }
        if (a < tries) await new Promise(res => setTimeout(res, 250));
    }
    return false;
}

async function uploadWebUi() {
    const fileEl = document.getElementById('www_files');
    const btn    = document.getElementById('www_btn');
    const list   = fileEl.files;
    if (!list.length) { showStatusEl('www_status', '❌ Select one or more files first', 'error'); return; }
    for (let j = 0; j < list.length; j++) {
        if (/\.bin$/i.test(list[j].name)) {
            showStatusEl('www_status', '❌ ' + list[j].name + ' looks like firmware (.bin). ' +
                         'Use the “Firmware update (OTA)” section above.', 'error');
            return;
        }
    }

    btn.disabled = true;
    let ok = 0;
    const failed = [];
    for (let i = 0; i < list.length; i++) {
        const file = list[i];
        showStatusEl('www_status', 'Uploading ' + file.name + ' (' + (i + 1) + '/' + list.length + ')…', '');
        if (await putWithRetry(file)) ok++; else failed.push(file.name);
    }
    btn.disabled = false;
    showStatusEl('www_status', 'Done: ' + ok + ' uploaded' +
                 (failed.length ? ', ' + failed.length + ' failed (' + failed.join(', ') + ')' : '') +
                 '. Reload the page to use the new UI.', failed.length > 0 ? 'error' : 'ok');
    try {
        const st = await (await fetch('/api/state', { cache: 'no-store' })).json();
        showWwwState(st);
    } catch (_) { /* leave the warning as-is on error */ }
}

// Set text + state class on an arbitrary status element (mirrors showOtaStatus).
function showStatusEl(id, msg, kind) {
    const el = document.getElementById(id);
    if (!el) return;
    el.textContent = msg;
    el.className = 'save-status' + (kind ? ' ' + kind : '');
}

// ── Settings backup / restore ────────────────────────────────────────────────
// Bundle every file on the config partition into one JSON. The config files are
// listed via /api/files?root=config and read through the static handler (it falls
// back to the config root), then written back via PUT /api/files/<name>?root=config.
// Layout-independent: unlike a raw partition dump it survives partition resizes,
// which is exactly the v0.38.0 (128 KB→1 MB) case that erases user data on flash.
const CFG_BACKUP_MAGIC = 'atlascube-settings';

async function exportSettings() {
    const btn = document.getElementById('cfg_export_btn');
    btn.disabled = true;
    showStatusEl('cfg_export_status', 'Reading config files…', '');
    try {
        const all = await (await fetch('/api/files?root=config', { cache: 'no-store' })).json();
        // Only .json config files belong in a backup — skip runtime leftovers
        // like playlist.csv or update.log that live on the same partition.
        const list = Array.isArray(all) ? all.filter(f => /\.json$/i.test(f.name)) : [];
        if (!list.length) {
            showStatusEl('cfg_export_status', 'Nothing to export.', 'error');
            return;
        }
        const files = {};
        for (let i = 0; i < list.length; i++) {
            const name = list[i].name;
            showStatusEl('cfg_export_status', 'Reading ' + name + ' (' + (i + 1) + '/' + list.length + ')…', '');
            const r = await fetch('/' + encodeURIComponent(name), { cache: 'no-store' });
            if (!r.ok) throw new Error('read ' + name + ' → HTTP ' + r.status);
            files[name] = await r.text();
        }
        const bundle = { format: CFG_BACKUP_MAGIC, version: 1, created: new Date().toISOString(), files };
        const blob = new Blob([JSON.stringify(bundle, null, 2)], { type: 'application/json' });
        const stamp = bundle.created.slice(0, 19).replace(/[:T]/g, '-');
        const a = document.createElement('a');
        a.href = URL.createObjectURL(blob);
        a.download = 'atlascube-settings-' + stamp + '.json';
        document.body.appendChild(a);
        a.click();
        a.remove();
        URL.revokeObjectURL(a.href);
        showStatusEl('cfg_export_status', '✅ Exported ' + list.length + ' files.', 'ok');
    } catch (e) {
        showStatusEl('cfg_export_status', '❌ Export failed: ' + e.message, 'error');
    } finally {
        btn.disabled = false;
    }
}

async function importSettings() {
    const fileEl = document.getElementById('cfg_import_file');
    const btn    = document.getElementById('cfg_import_btn');
    const f = fileEl.files[0];
    if (!f) { showStatusEl('cfg_import_status', '❌ Choose a backup .json first', 'error'); return; }

    let bundle;
    try {
        bundle = JSON.parse(await f.text());
    } catch (_) {
        showStatusEl('cfg_import_status', '❌ Not valid JSON', 'error');
        return;
    }
    if (!bundle || bundle.format !== CFG_BACKUP_MAGIC || !bundle.files || typeof bundle.files !== 'object') {
        showStatusEl('cfg_import_status', '❌ Not an AtlasCube settings backup', 'error');
        return;
    }
    // Same .json-only rule as export: old backups may carry playlist.csv or
    // update.log — don't restore those onto the config partition.
    const names = Object.keys(bundle.files).filter(n => /\.json$/i.test(n));
    if (!names.length) { showStatusEl('cfg_import_status', '❌ Backup is empty', 'error'); return; }

    btn.disabled = true;
    let ok = 0, fail = 0;
    for (let i = 0; i < names.length; i++) {
        const name = names[i];
        showStatusEl('cfg_import_status', 'Restoring ' + name + ' (' + (i + 1) + '/' + names.length + ')…', '');
        try {
            const r = await fetch('/api/files/' + encodeURIComponent(name) + '?root=config',
                                  { method: 'PUT', body: bundle.files[name] });
            if (r.ok) ok++; else fail++;
        } catch (_) { fail++; }
    }
    btn.disabled = false;
    showStatusEl('cfg_import_status',
                 'Done: ' + ok + ' restored' + (fail ? ', ' + fail + ' failed' : '') +
                 '. Restart the device to apply.', fail > 0 ? 'error' : 'ok');
}

// ─────────────────────────────────────────────────────────────────────────────
// Init
// ─────────────────────────────────────────────────────────────────────────────
initTabs();
loadSettings();
loadColors();
loadMqtt();
loadWeather();
loadNetWpMeta();
