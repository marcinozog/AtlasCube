'use strict';

// ─────────────────────────────────────────────────────────────────────────────
// AtlasCube — Playlist editor
// ─────────────────────────────────────────────────────────────────────────────

const PL_MAX      = 64;
const PL_NAME_MAX = 63;
const PL_URL_MAX  = 255;

let stations = [];   // [{name, url, favorite}, ...]
let dragIdx  = -1;   // index of the row being dragged

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
        }))
        .filter(s => s.name && s.url);

    if (clean.length === 0) {
        setStatus('Playlist is empty', 'error');
        return;
    }
    if (clean.length > PL_MAX) {
        setStatus(`Too many stations (max ${PL_MAX})`, 'error');
        return;
    }
    for (const s of clean) {
        if (s.name.length > PL_NAME_MAX) {
            setStatus(`Name too long: ${s.name}`, 'error');
            return;
        }
        if (s.url.length > PL_URL_MAX) {
            setStatus(`URL too long: ${s.name}`, 'error');
            return;
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
        setStatus('✓ Saved', 'ok');
    } catch (e) {
        console.error(e);
        setStatus('Save failed: ' + e.message, 'error');
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

    stations.forEach((st, i) => list.appendChild(makeRow(st, i)));
    updateMeta();
}

function makeRow(st, idx) {
    const row = document.createElement('div');
    row.className = 'pl-row';
    row.draggable = true;
    row.dataset.idx = idx;
    const fav = !!st.favorite;
    row.dataset.fav = fav ? '1' : '0';

    row.innerHTML = `
        <span class="pl-grip" title="Drag to reorder">⠿</span>
        <span class="pl-index">${idx + 1}.</span>
        <button class="pl-fav${fav ? ' on' : ''}" title="Mark as favorite"
                onclick="plToggleFav(this)">${fav ? '★' : '☆'}</button>
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
    stations.push({ name: '', url: '', favorite: false });
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
// Init
// ─────────────────────────────────────────────────────────────────────────────
plLoad();
