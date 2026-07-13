'use strict';

// ─────────────────────────────────────────────────────────────────────────────
// AtlasCube — Playlist editor
// ─────────────────────────────────────────────────────────────────────────────

const PL_MAX      = 512;
const PL_NAME_MAX = 63;
const PL_URL_MAX  = 255;
const PL_UUID_MAX = 36;
const PL_ICON_MAX = 127;
const ICON_SIZE   = 64;

let stations = [];   // [{name, url, favorite, stationuuid, icon}, ...]
let dragIdx  = -1;   // index of the row being dragged
let iconEditIdx = -1;
let iconCandidates = [];
let iconObserver = null;
let savedPlaylistSnapshot = '';
let allowUnloadOnce = false;
const iconThumbCache = new Map();

function playlistSnapshot() {
    return JSON.stringify(stations.map(s => ({
        name:        s.name || '',
        url:         s.url || '',
        favorite:    !!s.favorite,
        stationuuid: s.stationuuid || '',
        icon:        s.icon || '',
    })));
}

// ─────────────────────────────────────────────────────────────────────────────
// Load / Save
// ─────────────────────────────────────────────────────────────────────────────
async function plLoad() {
    try {
        const res = await fetch('/api/playlist', { cache: 'no-store' });
        stations = await res.json();
    } catch (e) {
        console.error('Playlist load error', e);
        setStatus('Failed to load playlist', 'error');
        stations = [];
    }
    sortFavoritesFirst();
    render();
    savedPlaylistSnapshot = playlistSnapshot();
}

// Stable sort: favorites first, original order preserved within each group.
// Called after any operation that may have placed a favorite below a non-favorite.
function sortFavoritesFirst() {
    stations.sort((a, b) => (b.favorite ? 1 : 0) - (a.favorite ? 1 : 0));
}

async function plSave() {
    // Sync DOM → model before save
    syncFromDom();

    // Client-side validation
    const clean = stations
        .map(s => ({
            name:     (s.name || '').trim(),
            url:      (s.url  || '').trim(),
            favorite: !!s.favorite,
            stationuuid: (s.stationuuid || '').trim(),
            icon:        (s.icon || '').trim(),
        }))
        .filter(s => s.name && s.url);

    if (clean.length === 0) {
        setStatus('Playlist is empty', 'error');
        return false;
    }
    if (clean.length > PL_MAX) {
        setStatus(`Too many stations (max ${PL_MAX})`, 'error');
        return false;
    }
    for (const s of clean) {
        if (s.name.length > PL_NAME_MAX) {
            setStatus(`Name too long: ${s.name}`, 'error');
            return false;
        }
        if (s.url.length > PL_URL_MAX) {
            setStatus(`URL too long: ${s.name}`, 'error');
            return false;
        }
        if (s.stationuuid.length > PL_UUID_MAX || s.icon.length > PL_ICON_MAX) {
            setStatus(`Icon metadata too long: ${s.name}`, 'error');
            return false;
        }
    }

    const btn = document.getElementById('pl_save_btn');
    btn.disabled = true;
    setStatus('Saving…', '');

    try {
        const res = await fetch('/api/playlist', {
            method:  'POST',
            headers: { 'Content-Type': 'application/json' },
            body:    JSON.stringify(clean),
        });
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        stations = clean;
        render();
        savedPlaylistSnapshot = playlistSnapshot();
        setStatus('✓ Saved', 'ok');
        return true;
    } catch (e) {
        console.error(e);
        setStatus('Save failed: ' + e.message, 'error');
        return false;
    } finally {
        btn.disabled = false;
    }
}

function setStatus(text, cls) {
    const el = document.getElementById('pl_status');
    el.textContent = text;
    el.className   = 'save-status' + (cls ? ' ' + cls : '');
}

// ─────────────────────────────────────────────────────────────────────────────
// Model ↔ DOM sync
// ─────────────────────────────────────────────────────────────────────────────
function syncFromDom() {
    const rows = document.querySelectorAll('#pl_list .pl-row');
    stations = Array.from(rows).map(row => ({
        name:     row.querySelector('input.name').value,
        url:      row.querySelector('input.url').value,
        favorite: row.dataset.fav === '1',
        stationuuid: row.dataset.uuid || '',
        icon:        row.dataset.icon || '',
    }));
}

function updateMeta() {
    document.getElementById('pl_count').textContent = stations.length;
    document.getElementById('pl_max').textContent   = PL_MAX;
    document.getElementById('pl_max2').textContent  = PL_MAX;
}

// ─────────────────────────────────────────────────────────────────────────────
// Render
// ─────────────────────────────────────────────────────────────────────────────
function render() {
    const list = document.getElementById('pl_list');
    list.innerHTML = '';
    if (iconObserver) iconObserver.disconnect();
    iconObserver = 'IntersectionObserver' in window
        ? new IntersectionObserver(entries => entries.forEach(e => {
            if (e.isIntersecting) {
                iconHydrateThumb(e.target);
                iconObserver.unobserve(e.target);
            }
        }), { root: list, rootMargin: '80px' })
        : null;

    stations.forEach((st, i) => list.appendChild(makeRow(st, i)));
    updateMeta();
}

function makeRow(st, idx) {
    const row = document.createElement('div');
    row.className = 'pl-row';
    row.draggable = false;   // enabled only while grabbing the grip
    row.dataset.idx = idx;
    const fav = !!st.favorite;
    row.dataset.fav = fav ? '1' : '0';
    row.dataset.uuid = st.stationuuid || '';
    row.dataset.icon = st.icon || '';

    row.innerHTML = `
        <span class="pl-grip" title="Drag to reorder">⠿</span>
        <span class="pl-index">${idx + 1}.</span>
        <button class="pl-fav${fav ? ' on' : ''}" title="Mark as favorite"
                onclick="plToggleFav(this)">${fav ? '★' : '☆'}</button>
        <button class="pl-icon${st.icon ? ' has-icon' : ''}" type="button"
                title="${st.icon ? 'Change station icon' : 'Add station icon'}"
                data-icon-path="${escapeAttr(st.icon || '')}" onclick="iconOpen(${idx})">${st.icon ? '●' : '+'}</button>
        <input type="text" class="name" placeholder="Station name"
               maxlength="${PL_NAME_MAX}" value="${escapeAttr(st.name || '')}" />
        <input type="text" class="url"  placeholder="https://…"
               maxlength="${PL_URL_MAX}"  value="${escapeAttr(st.url  || '')}" />
        <button class="pl-del" title="Delete" onclick="plDelRow(this)">✕</button>
    `;

    // Drag-and-drop handlers
    row.addEventListener('dragstart', onDragStart);
    row.addEventListener('dragend',   onDragEnd);
    row.addEventListener('dragover',  onDragOver);
    row.addEventListener('dragleave', onDragLeave);
    row.addEventListener('drop',      onDrop);

    // Make the row draggable only when grabbing the grip, so text
    // selection inside the name/url inputs keeps working.
    const grip = row.querySelector('.pl-grip');
    grip.addEventListener('mousedown',  () => { row.draggable = true; });
    grip.addEventListener('touchstart', () => { row.draggable = true; }, { passive: true });
    row.addEventListener('mouseup',     () => { row.draggable = false; });

    const iconBtn = row.querySelector('.pl-icon');
    if (st.icon) {
        if (iconObserver) iconObserver.observe(iconBtn);
        else iconHydrateThumb(iconBtn);
    }

    return row;
}

function escapeAttr(s) {
    return String(s).replace(/&/g, '&amp;').replace(/"/g, '&quot;').replace(/</g, '&lt;');
}

// ─────────────────────────────────────────────────────────────────────────────
// Row operations
// ─────────────────────────────────────────────────────────────────────────────
function plAddRow() {
    syncFromDom();
    if (stations.length >= PL_MAX) {
        setStatus(`Max ${PL_MAX} stations reached`, 'error');
        return;
    }
    stations.push({ name: '', url: '', favorite: false, stationuuid: '', icon: '' });
    render();
    // Focus the new name field
    const rows = document.querySelectorAll('#pl_list .pl-row');
    const last = rows[rows.length - 1];
    if (last) {
        last.querySelector('input.name').focus();
        last.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
    }
}

function plDelRow(btn) {
    const row = btn.closest('.pl-row');
    const idx = parseInt(row.dataset.idx, 10);
    syncFromDom();
    stations.splice(idx, 1);
    render();
}

// ─────────────────────────────────────────────────────────────────────────────
// Import — replaces the current list (in the editor only; user must Save)
// ─────────────────────────────────────────────────────────────────────────────
async function plImport(input) {
    const file = input.files && input.files[0];
    input.value = '';   // allow re-importing the same file later
    if (!file) return;

    let text;
    try {
        text = await file.text();
    } catch (e) {
        setStatus('Failed to read file: ' + e.message, 'error');
        return;
    }
    if (text.charCodeAt(0) === 0xFEFF) text = text.slice(1);   // strip BOM

    const parsed = [];
    let skipped  = 0;
    for (const raw of text.split(/\r?\n/)) {
        const line = raw.trim();
        if (!line) continue;
        const cols = line.split('\t');
        if (cols.length < 2 || !cols[0].trim() || !cols[1].trim()) { skipped++; continue; }
        parsed.push({
            name:     cols[0].trim().slice(0, PL_NAME_MAX),
            url:      cols[1].trim().slice(0, PL_URL_MAX),
            favorite: cols.length > 2 && cols[2].trim() === '1',
            stationuuid: cols.length > 3 ? cols[3].trim().slice(0, PL_UUID_MAX) : '',
            icon:        cols.length > 4 ? cols[4].trim().slice(0, PL_ICON_MAX) : '',
        });
    }

    if (parsed.length === 0) {
        setStatus('No valid stations found (expected name\\turl per line)', 'error');
        return;
    }

    let truncated = 0;
    if (parsed.length > PL_MAX) {
        truncated = parsed.length - PL_MAX;
        parsed.length = PL_MAX;
    }

    if (!confirm(`Replace current list with ${parsed.length} stations from "${file.name}"?`)) {
        return;
    }

    stations = parsed;
    sortFavoritesFirst();
    render();

    let msg = `✓ Imported ${parsed.length} stations — review and Save to apply`;
    if (skipped)   msg += ` (${skipped} invalid lines skipped)`;
    if (truncated) msg += ` (${truncated} extra lines dropped, max ${PL_MAX})`;
    setStatus(msg, 'ok');
}

function plToggleFav(btn) {
    const row = btn.closest('.pl-row');
    const idx = parseInt(row.dataset.idx, 10);
    syncFromDom();
    stations[idx].favorite = !stations[idx].favorite;
    sortFavoritesFirst();
    render();
}

// ─────────────────────────────────────────────────────────────────────────────
// Drag-and-drop
// ─────────────────────────────────────────────────────────────────────────────
function onDragStart(e) {
    const row = e.currentTarget;
    dragIdx = parseInt(row.dataset.idx, 10);
    row.classList.add('dragging');
    // For Firefox — required to start a drag
    try { e.dataTransfer.setData('text/plain', String(dragIdx)); } catch {}
    e.dataTransfer.effectAllowed = 'move';
}

function onDragEnd(e) {
    e.currentTarget.draggable = false;
    e.currentTarget.classList.remove('dragging');
    clearDragHints();
    dragIdx = -1;
}

function onDragOver(e) {
    e.preventDefault();
    e.dataTransfer.dropEffect = 'move';

    const row = e.currentTarget;
    if (parseInt(row.dataset.idx, 10) === dragIdx) return;

    const rect = e.currentTarget.getBoundingClientRect();
    const before = (e.clientY - rect.top) < rect.height / 2;

    clearDragHints();
    row.classList.add(before ? 'drag-over-top' : 'drag-over-bottom');
}

function onDragLeave(e) {
    e.currentTarget.classList.remove('drag-over-top', 'drag-over-bottom');
}

function onDrop(e) {
    e.preventDefault();
    const row = e.currentTarget;
    const targetIdx = parseInt(row.dataset.idx, 10);
    if (dragIdx < 0 || dragIdx === targetIdx) { clearDragHints(); return; }

    const rect   = row.getBoundingClientRect();
    const before = (e.clientY - rect.top) < rect.height / 2;

    // Persist current DOM edits to model before reorder
    syncFromDom();

    const item = stations.splice(dragIdx, 1)[0];
    let insertAt = targetIdx + (before ? 0 : 1);
    // Adjust if removal was before the target
    if (dragIdx < targetIdx) insertAt--;
    stations.splice(insertAt, 0, item);

    // Keep favorites pinned to the top — drop within own group sticks,
    // drop across groups gets clamped back into the correct group.
    sortFavoritesFirst();

    clearDragHints();
    render();
}

function clearDragHints() {
    document.querySelectorAll('.pl-row').forEach(r =>
        r.classList.remove('drag-over-top', 'drag-over-bottom')
    );
}

// ─────────────────────────────────────────────────────────────────────────────
// Station search — radio-browser.info
// ─────────────────────────────────────────────────────────────────────────────
// Docs ask clients not to hardcode one mirror; `all.` is a round-robin DNS
// entry, the named mirrors below are fallbacks in case it is down.
const RB_HOSTS = [
    'all.api.radio-browser.info',
    'de1.api.radio-browser.info',
    'fi1.api.radio-browser.info',
];
const RB_LIMIT = 30;

let rbResults = [];   // last search results, including stationuuid + favicon

function rbTogglePanel() {
    const panel = document.getElementById('rb_panel');
    const toggle = document.querySelector('.rb-toggle');
    panel.hidden = !panel.hidden;
    document.getElementById('rb_arrow').textContent = panel.hidden ? '▸' : '▾';
    toggle.setAttribute('aria-expanded', String(!panel.hidden));
    if (!panel.hidden) document.getElementById('rb_query').focus();
}

async function rbSearch() {
    const query = document.getElementById('rb_query').value.trim();
    if (!query) return;
    const by  = document.getElementById('rb_by').value;   // name | tag | country
    const btn = document.getElementById('rb_search_btn');

    btn.disabled = true;
    rbSetStatus('Searching…', '');
    document.getElementById('rb_results').innerHTML = '';

    const params = `${by}=${encodeURIComponent(query)}` +
                   `&limit=${RB_LIMIT}&hidebroken=true&order=clickcount&reverse=true`;

    let list = null, lastErr = null;
    for (const host of RB_HOSTS) {
        try {
            const res = await fetch(`https://${host}/json/stations/search?${params}`);
            if (!res.ok) throw new Error(`HTTP ${res.status}`);
            list = await res.json();
            break;
        } catch (e) {
            lastErr = e;
        }
    }
    btn.disabled = false;

    if (!list) {
        rbSetStatus('Search failed: ' + (lastErr ? lastErr.message : 'no server reachable'), 'error');
        return;
    }

    let tooLong = 0;
    rbResults = list
        .map(st => ({
            // url_resolved has .pls/.m3u playlists already resolved to the stream
            name:    (st.name || '').trim().slice(0, PL_NAME_MAX),
            url:     (st.url_resolved || st.url || '').trim(),
            codec:   (st.codec || '?').toUpperCase(),
            bitrate: st.bitrate | 0,
            country: st.countrycode || '',
            stationuuid: st.stationuuid || '',
            favicon: (st.favicon || '').trim(),
        }))
        .filter(st => {
            if (!st.name || !st.url) return false;
            if (st.url.length > PL_URL_MAX) { tooLong++; return false; }
            return true;
        });

    rbRenderResults();

    if (rbResults.length === 0) {
        rbSetStatus('No stations found', 'error');
    } else {
        let msg = `${rbResults.length} stations`;
        if (tooLong) msg += ` (${tooLong} skipped — URL too long)`;
        rbSetStatus(msg, '');
    }
}

function rbRenderResults() {
    const box = document.getElementById('rb_results');
    box.innerHTML = '';

    syncFromDom();
    const existing = new Set(stations.map(s => (s.url || '').trim()));

    rbResults.forEach((st, i) => {
        const row = document.createElement('div');
        // Device decodes MP3/AAC — dim the rest, but leave them addable
        const supported = st.codec === 'MP3' || st.codec.startsWith('AAC');
        row.className = 'rb-row' + (supported ? '' : ' rb-unsupported');
        if (!supported) row.title = 'Codec may not be supported by the device';

        const added = existing.has(st.url);
        const meta  = `${st.codec}${st.bitrate ? ' ' + st.bitrate + 'k' : ''}` +
                      `${st.country ? ' · ' + st.country : ''}`;
        row.innerHTML = `
            ${st.favicon
                ? `<img class="rb-logo" src="${escapeAttr(st.favicon)}" alt="" loading="lazy" referrerpolicy="no-referrer">`
                : '<span class="rb-logo-fallback">◉</span>'}
            <span class="rb-name" title="${escapeAttr(st.url)}">${escapeAttr(st.name)}</span>
            <span class="rb-meta">${escapeAttr(meta)}</span>
            <button class="rb-add${added ? ' added' : ''}" ${added ? 'disabled' : ''}
                    onclick="rbAdd(${i}, this)">${added ? '✓' : '➕'}</button>
        `;
        box.appendChild(row);
    });
}

async function rbAdd(i, btn) {
    const st = rbResults[i];
    if (!st) return;

    syncFromDom();
    if (stations.length >= PL_MAX) {
        rbSetStatus(`Max ${PL_MAX} stations reached`, 'error');
        return;
    }
    if (stations.some(s => (s.url || '').trim() === st.url)) {
        btn.disabled = true;
        btn.textContent = '✓';
        btn.classList.add('added');
        return;
    }

    stations.push({
        name: st.name, url: st.url, favorite: false,
        stationuuid: st.stationuuid || '', icon: ''
    });
    const addedIdx = stations.length - 1;
    render();
    btn.disabled = true;
    btn.textContent = '✓';
    btn.classList.add('added');
    rbSetStatus(`✓ "${st.name}" added${st.favicon ? ' — importing icon…' : ''}`, 'ok');
    if (st.favicon) {
        try {
            await iconImportRemoteForIndex(addedIdx, st.favicon, st.stationuuid);
            rbSetStatus(`✓ "${st.name}" and its icon added — Save & Apply to keep it`, 'ok');
        } catch (e) {
            rbSetStatus(`Station added, but icon import failed: ${e.message}`, 'error');
        }
    } else {
        rbSetStatus(`✓ "${st.name}" added — Save & Apply to keep it`, 'ok');
    }
}

function rbSetStatus(text, cls) {
    const el = document.getElementById('rb_status');
    el.textContent = text;
    el.className   = 'rb-status' + (cls ? ' ' + cls : '');
}

// ─────────────────────────────────────────────────────────────────────────────
// Station icons — Radio Browser lookup / local upload → LVGL RGB565 on SD
// ─────────────────────────────────────────────────────────────────────────────

function iconSetStatus(text, cls = '') {
    const el = document.getElementById('icon_status');
    el.textContent = text;
    el.className = 'icon-status' + (cls ? ' ' + cls : '');
}

async function iconHydrateThumb(btn) {
    const path = btn.dataset.iconPath;
    if (!path) return;
    try {
        let dataUrl = iconThumbCache.get(path);
        if (!dataUrl) {
            const r = await fetch('/api/sd/file?path=' + encodeURIComponent(path), { cache: 'no-store' });
            if (!r.ok) throw new Error('HTTP ' + r.status);
            const decoded = LvBin.decodeToCanvas(await r.arrayBuffer());
            dataUrl = decoded.canvas.toDataURL('image/png');
            iconThumbCache.set(path, dataUrl);
        }
        btn.style.backgroundImage = `url("${dataUrl}")`;
        btn.textContent = '';
    } catch (_) {
        btn.classList.add('icon-missing');
        btn.classList.remove('has-icon');
        btn.textContent = '!';
        btn.title = 'Assigned icon is missing from the SD card';
    }
}

function iconOpen(idx) {
    syncFromDom();
    if (idx < 0 || idx >= stations.length) return;
    iconEditIdx = idx;
    iconCandidates = [];
    const st = stations[idx];
    document.getElementById('icon_station_name').textContent = st.name || 'Unnamed station';
    document.getElementById('icon_current_path').textContent = st.icon || 'No icon assigned';
    document.getElementById('icon_results').innerHTML = '';
    document.getElementById('icon_modal').hidden = false;
    iconSetStatus('');
    iconRefreshPreview();
}

function iconClose() {
    document.getElementById('icon_modal').hidden = true;
    document.getElementById('icon_results').innerHTML = '';
    iconEditIdx = -1;
    iconCandidates = [];
}

async function iconRefreshPreview() {
    const preview = document.getElementById('icon_preview');
    preview.innerHTML = '◉';
    const st = stations[iconEditIdx];
    if (!st || !st.icon) return;
    try {
        const r = await fetch('/api/sd/file?path=' + encodeURIComponent(st.icon), { cache: 'no-store' });
        if (r.status === 503) throw new Error('no SD card');
        if (!r.ok) throw new Error('HTTP ' + r.status);
        const { canvas } = LvBin.decodeToCanvas(await r.arrayBuffer());
        preview.innerHTML = '';
        preview.appendChild(canvas);
    } catch (e) {
        preview.textContent = '!';
        iconSetStatus('Assigned icon cannot be read: ' + e.message, 'error');
    }
}

async function iconRbGet(path) {
    let lastError = null;
    for (const host of RB_HOSTS) {
        try {
            const r = await fetch(`https://${host}${path}`);
            if (!r.ok) throw new Error('HTTP ' + r.status);
            return await r.json();
        } catch (e) {
            lastError = e;
        }
    }
    throw lastError || new Error('Radio Browser unavailable');
}

async function iconLookup(st, allowNameFallback = true) {
    let list = await iconRbGet('/json/stations/byurl?url=' + encodeURIComponent(st.url) +
                               '&hidebroken=true&limit=20');
    let exact = true;
    if ((!Array.isArray(list) || list.length === 0) && allowNameFallback && st.name) {
        exact = false;
        list = await iconRbGet('/json/stations/bynameexact/' + encodeURIComponent(st.name) +
                               '?hidebroken=true&limit=20');
    }
    const seen = new Set();
    const matches = (Array.isArray(list) ? list : [])
        .filter(x => x && x.favicon)
        .filter(x => {
            const key = x.stationuuid || x.favicon;
            if (seen.has(key)) return false;
            seen.add(key);
            return true;
        })
        .sort((a, b) => (b.clickcount | 0) - (a.clickcount | 0));
    return { matches, exact };
}

async function iconFindCandidates() {
    if (iconEditIdx < 0) return;
    const btn = document.getElementById('icon_find_btn');
    btn.disabled = true;
    iconSetStatus('Searching Radio Browser…');
    document.getElementById('icon_results').innerHTML = '';
    try {
        const found = await iconLookup(stations[iconEditIdx], true);
        iconCandidates = found.matches;
        iconRenderCandidates();
        if (!iconCandidates.length) {
            iconSetStatus('No station image found. You can upload one manually.', 'error');
        } else {
            iconSetStatus(found.exact
                ? `${iconCandidates.length} URL match${iconCandidates.length === 1 ? '' : 'es'} found.`
                : `${iconCandidates.length} name match${iconCandidates.length === 1 ? '' : 'es'} found — choose carefully.`);
        }
    } catch (e) {
        iconSetStatus('Search failed: ' + e.message, 'error');
    } finally {
        btn.disabled = false;
    }
}

function iconRenderCandidates() {
    const box = document.getElementById('icon_results');
    box.innerHTML = '';
    iconCandidates.forEach((st, idx) => {
        const row = document.createElement('div');
        row.className = 'icon-candidate';
        const meta = [st.codec, st.bitrate ? st.bitrate + 'k' : '', st.countrycode]
            .filter(Boolean).join(' · ');
        row.innerHTML = `
            <img class="icon-candidate-logo" src="${escapeAttr(st.favicon)}" alt="" referrerpolicy="no-referrer">
            <div><div class="icon-candidate-name">${escapeAttr(st.name || 'Unknown station')}</div>
                 <div class="icon-candidate-meta">${escapeAttr(meta)}</div></div>
            <button class="btn-secondary" type="button" onclick="iconUseCandidate(${idx})">Use</button>`;
        box.appendChild(row);
    });
}

async function iconUseCandidate(idx) {
    const candidate = iconCandidates[idx];
    if (!candidate || iconEditIdx < 0) return;
    iconSetStatus('Downloading and converting image…');
    try {
        await iconImportRemoteForIndex(iconEditIdx, candidate.favicon, candidate.stationuuid || '');
        iconSetStatus('✓ Icon uploaded to SD. Save & Apply to keep the assignment.', 'ok');
        document.getElementById('icon_current_path').textContent = stations[iconEditIdx].icon;
        await iconRefreshPreview();
    } catch (e) {
        iconSetStatus('Icon import failed: ' + e.message, 'error');
    }
}

async function iconFetchRemoteBlob(url) {
    // Prefer a direct CORS fetch; fall back to AtlasCube's tightly capped proxy
    // for the many favicon hosts that do not expose Access-Control-Allow-Origin.
    try {
        const direct = await fetch(url, { mode: 'cors', cache: 'no-store' });
        if (direct.ok) return await direct.blob();
    } catch (_) { /* proxy fallback */ }
    const proxied = await fetch('/api/station-icon/proxy?url=' + encodeURIComponent(url),
                                { cache: 'no-store' });
    if (!proxied.ok) {
        const reason = await proxied.text().catch(() => 'HTTP ' + proxied.status);
        throw new Error(reason || 'HTTP ' + proxied.status);
    }
    return await proxied.blob();
}

async function iconDecodeSource(blob) {
    if ('createImageBitmap' in window) {
        try { return await createImageBitmap(blob); }
        catch (_) { /* HTMLImageElement fallback handles a few more formats */ }
    }
    return await new Promise((resolve, reject) => {
        const url = URL.createObjectURL(blob);
        const img = new Image();
        img.onload = () => { URL.revokeObjectURL(url); resolve(img); };
        img.onerror = () => { URL.revokeObjectURL(url); reject(new Error('browser cannot decode this image')); };
        img.src = url;
    });
}

async function iconToLvBin(blob) {
    const src = await iconDecodeSource(blob);
    const sw = src.width || src.naturalWidth;
    const sh = src.height || src.naturalHeight;
    if (!sw || !sh) throw new Error('image has invalid dimensions');

    const canvas = document.createElement('canvas');
    canvas.width = canvas.height = ICON_SIZE;
    const ctx = canvas.getContext('2d', { alpha: false });
    ctx.fillStyle = '#ffffff';
    ctx.fillRect(0, 0, ICON_SIZE, ICON_SIZE);
    ctx.imageSmoothingEnabled = true;
    ctx.imageSmoothingQuality = 'high';
    const pad = 5;
    const scale = Math.min((ICON_SIZE - pad * 2) / sw, (ICON_SIZE - pad * 2) / sh);
    const dw = Math.max(1, Math.round(sw * scale));
    const dh = Math.max(1, Math.round(sh * scale));
    ctx.drawImage(src, Math.round((ICON_SIZE - dw) / 2), Math.round((ICON_SIZE - dh) / 2), dw, dh);
    if (typeof src.close === 'function') src.close();

    const rgba = ctx.getImageData(0, 0, ICON_SIZE, ICON_SIZE).data;
    const out = new ArrayBuffer(12 + ICON_SIZE * ICON_SIZE * 2);
    const dv = new DataView(out);
    dv.setUint8(0, 0x19);
    dv.setUint8(1, 0x12);
    dv.setUint16(2, 0, true);
    dv.setUint16(4, ICON_SIZE, true);
    dv.setUint16(6, ICON_SIZE, true);
    dv.setUint16(8, ICON_SIZE * 2, true);
    dv.setUint16(10, 0, true);
    let p = 12;
    for (let i = 0; i < rgba.length; i += 4) {
        const rgb565 = ((rgba[i] & 0xf8) << 8) |
                       ((rgba[i + 1] & 0xfc) << 3) |
                       (rgba[i + 2] >> 3);
        dv.setUint16(p, rgb565, true);
        p += 2;
    }
    return new Blob([out], { type: 'application/octet-stream' });
}

function iconStorageKey(st, uuid) {
    const candidate = String(uuid || st.stationuuid || '').toLowerCase();
    if (/^[a-z0-9-]{8,64}$/.test(candidate)) return candidate;
    // Stable FNV-1a key shared only by the web editor; the resolved path is
    // stored in playlist.csv, so firmware does not need to reproduce the hash.
    let h = 0x811c9dc5;
    const text = (st.url || st.name || 'station').trim().toLowerCase();
    for (let i = 0; i < text.length; i++) {
        h ^= text.charCodeAt(i);
        h = Math.imul(h, 0x01000193);
    }
    return 'custom-' + (h >>> 0).toString(16).padStart(8, '0');
}

async function iconImportBlobForIndex(idx, sourceBlob, uuid = '') {
    syncFromDom();
    const st = stations[idx];
    if (!st) throw new Error('station no longer exists');
    const bin = await iconToLvBin(sourceBlob);
    const key = iconStorageKey(st, uuid);
    const path = '/station-icons/' + key + '.bin';
    const upload = await fetch('/api/sd/file?path=' + encodeURIComponent(path), {
        method: 'POST', body: bin
    });
    if (upload.status === 503) throw new Error('no SD card');
    if (!upload.ok) throw new Error('SD upload HTTP ' + upload.status);
    st.stationuuid = (uuid || st.stationuuid || '').slice(0, PL_UUID_MAX);
    st.icon = path;
    iconThumbCache.delete(path);
    render();
    return path;
}

async function iconImportRemoteForIndex(idx, url, uuid = '') {
    const source = await iconFetchRemoteBlob(url);
    return await iconImportBlobForIndex(idx, source, uuid);
}

async function iconUploadSelected(input) {
    const file = input.files && input.files[0];
    input.value = '';
    if (!file || iconEditIdx < 0) return;
    iconSetStatus('Converting and uploading image…');
    try {
        await iconImportBlobForIndex(iconEditIdx, file);
        document.getElementById('icon_current_path').textContent = stations[iconEditIdx].icon;
        await iconRefreshPreview();
        iconSetStatus('✓ Icon uploaded to SD. Save & Apply to keep the assignment.', 'ok');
    } catch (e) {
        iconSetStatus('Upload failed: ' + e.message, 'error');
    }
}

function iconRemove() {
    if (iconEditIdx < 0 || !stations[iconEditIdx]) return;
    stations[iconEditIdx].icon = '';
    document.getElementById('icon_current_path').textContent = 'No icon assigned';
    document.getElementById('icon_preview').innerHTML = '◉';
    render();
    iconSetStatus('Assignment removed. The SD file was left intact; Save & Apply to keep this change.', 'ok');
}

async function iconFindMissing() {
    syncFromDom();
    const missing = stations.map((st, idx) => ({ st, idx })).filter(x => !x.st.icon);
    if (!missing.length) {
        setStatus('Every station already has an icon assignment', 'ok');
        return;
    }
    const btn = document.getElementById('icons_missing_btn');
    btn.disabled = true;
    let imported = 0, ambiguous = 0, failed = 0;
    try {
        for (let i = 0; i < missing.length; i++) {
            const { st, idx } = missing[i];
            setStatus(`Finding icons ${i + 1}/${missing.length}: ${st.name}…`, '');
            try {
                const found = await iconLookup(st, true);
                // URL matches are safe to choose by popularity. A name-only
                // fallback is imported only when exactly one candidate exists.
                const candidate = found.exact ? found.matches[0]
                                : found.matches.length === 1 ? found.matches[0] : null;
                if (!candidate) { ambiguous++; continue; }
                await iconImportRemoteForIndex(idx, candidate.favicon, candidate.stationuuid || '');
                imported++;
            } catch (_) {
                failed++;
            }
        }
        setStatus(`Icon scan finished: ${imported} uploaded, ${ambiguous} need manual choice, ${failed} failed. Save & Apply to keep assignments.`,
                  failed ? 'error' : 'ok');
    } finally {
        btn.disabled = false;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Init
// ─────────────────────────────────────────────────────────────────────────────
plLoad();
if (location.hash === '#search') rbTogglePanel();   // deep-link from index.html
document.getElementById('icon_modal').addEventListener('click', e => {
    if (e.target.id === 'icon_modal') iconClose();
});
document.addEventListener('keydown', e => {
    if (e.key === 'Escape' && !document.getElementById('icon_modal').hidden) iconClose();
});
window.addEventListener('beforeunload', e => {
    if (allowUnloadOnce) return;
    syncFromDom();
    if (playlistSnapshot() === savedPlaylistSnapshot) return;
    e.preventDefault();
    e.returnValue = '';
});
document.addEventListener('click', async e => {
    const link = e.target.closest('a[href]');
    if (!link || link.hasAttribute('download') || link.target === '_blank') return;
    const href = link.getAttribute('href');
    if (!href || href.startsWith('#') || href.startsWith('javascript:')) return;

    syncFromDom();
    if (playlistSnapshot() === savedPlaylistSnapshot) return;

    e.preventDefault();
    if (!window.confirm('The playlist has unsaved changes. Save & Apply before leaving?\n\nOK = save and leave   Cancel = stay on this page')) return;
    if (await plSave()) {
        allowUnloadOnce = true;
        location.href = link.href;
    }
});
