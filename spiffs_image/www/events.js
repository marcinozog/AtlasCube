'use strict';

// ─────────────────────────────────────────────────────────────────────────────
// AtlasCube — Events editor
// ─────────────────────────────────────────────────────────────────────────────

const TYPE_ICON = {
    birthday:    '🎂',
    nameday:     '🌹',
    anniversary: '💍',
    reminder:    '⏰',
    alarm:       '⏰',
    voice:       '🗣️',
};

const TYPE_LABEL = {
    birthday:    'Birthday',
    nameday:     'Name day',
    anniversary: 'Anniversary',
    reminder:    'Reminder',
    alarm:       'Alarm',
    voice:       'Voice',
};

const REC_LABEL = {
    none:    'one-time',
    daily:   'daily',
    weekly:  'weekly',
    monthly: 'monthly',
    yearly:  'yearly',
};

let events    = [];
let stations  = [];      // playlist entries, fetched once for the alarm picker
let editingId = null;    // null = add mode, string = edit mode
let clipUrl   = null;    // object URL of the loaded voice clip (revoked on reset)

async function loadStations() {
    try {
        const res = await fetch('/api/playlist', { cache: 'no-store' });
        stations = await res.json();
    } catch (e) {
        console.error('Playlist load error', e);
        stations = [];
    }
    const sel = document.getElementById('ev_station');
    sel.innerHTML = stations.length
        ? stations.map((s, i) =>
            `<option value="${i}">${i + 1}. ${escapeHtml(s.name || '(unnamed)')}</option>`).join('')
        : '<option value="0" disabled>Playlist empty — add stations first</option>';
}

function evTypeChanged() {
    const type    = document.getElementById('ev_type').value;
    const isAlarm = type === 'alarm';
    const isVoice = type === 'voice';
    document.getElementById('ev_station_group').style.display = isAlarm ? '' : 'none';
    document.getElementById('ev_volume_group').style.display  = (isAlarm || isVoice) ? '' : 'none';
    document.getElementById('ev_sound_group').style.display   = isVoice ? '' : 'none';
}

// Stop playback and drop the loaded clip (called when the form changes events).
function resetClipAudio() {
    const audio = document.getElementById('ev_sound_audio');
    audio.pause();
    audio.removeAttribute('src');
    audio.load();
    if (clipUrl) { URL.revokeObjectURL(clipUrl); clipUrl = null; }
    document.getElementById('ev_sound_play').textContent = '▶ Play';
}

// Play/stop the voice clip. Loaded lazily as a blob so the device's
// Content-Type/Content-Disposition (octet-stream/attachment) don't block <audio>.
async function evPlayClip() {
    const audio = document.getElementById('ev_sound_audio');
    const sound = document.getElementById('ev_sound').value;
    if (!sound) return;
    if (!audio.paused) { audio.pause(); return; }
    if (!audio.getAttribute('src')) {
        try {
            const res = await fetch('/api/sd/file?path=' +
                                    encodeURIComponent('/voice/' + sound), { cache: 'no-store' });
            if (!res.ok) { setStatus('Audio not found on SD', 'error'); return; }
            clipUrl = URL.createObjectURL(await res.blob());
            audio.src = clipUrl;
        } catch (e) {
            setStatus('Audio load failed', 'error');
            return;
        }
    }
    audio.play();
}

// ─────────────────────────────────────────────────────────────────────────────
// Load / Render
// ─────────────────────────────────────────────────────────────────────────────
async function evLoad() {
    try {
        const res = await fetch('/api/events', { cache: 'no-store' });
        events = await res.json();
    } catch (e) {
        console.error('Events load error', e);
        setStatus('Failed to load events', 'error');
        events = [];
    }
    render();
}

function render() {
    const list = document.getElementById('ev_list');
    list.innerHTML = '';
    document.getElementById('ev_count').textContent = events.length;

    if (events.length === 0) {
        list.innerHTML = '<div class="pl-hint" style="text-align:center;padding:20px 0">No events yet. Fill the form above and click Save.</div>';
        return;
    }

    events
        .slice()
        .sort(sortByNextOccurrence)
        .forEach(ev => list.appendChild(makeRow(ev)));
}

function makeRow(ev) {
    const row = document.createElement('div');
    row.className = 'ev-row' + (ev.enabled ? '' : ' disabled');

    const dateStr = formatDate(ev);
    const timeStr = `${pad2(ev.hour)}:${pad2(ev.minute)}`;
    const recStr  = REC_LABEL[ev.recurrence] || ev.recurrence;

    let extra = '';
    if (ev.type === 'alarm') {
        const st = stations[ev.station];
        const name = st ? st.name : `#${ev.station}`;
        extra = ` · 📻 ${escapeHtml(name)} · 🔊 ${ev.volume ?? 0}`;
    } else if (ev.type === 'voice') {
        extra = ` · 🗣️ ${escapeHtml(ev.sound || '(no audio)')} · 🔊 ${ev.volume ?? 0}`;
    }

    row.innerHTML = `
        <div class="ev-icon">${TYPE_ICON[ev.type] || '•'}</div>
        <div class="ev-info">
            <div class="ev-title">${escapeHtml(ev.title)}</div>
            <div class="ev-meta">${dateStr} · ${timeStr}${extra}</div>
        </div>
        <span class="ev-tag">${recStr}</span>
        <label class="ev-tag" style="cursor:pointer">
            <input type="checkbox" ${ev.enabled ? 'checked' : ''}
                   onchange="evToggleEnabled('${ev.id}', this.checked)" />
            on
        </label>
        <button class="ev-btn" title="Edit" onclick="evEdit('${ev.id}')">✎</button>
        <button class="ev-btn danger" title="Delete" onclick="evDelete('${ev.id}')">✕</button>
    `;
    return row;
}

function formatDate(ev) {
    const d = `${ev.year}-${pad2(ev.month)}-${pad2(ev.day)}`;
    if (ev.recurrence === 'yearly')  return `${pad2(ev.month)}-${pad2(ev.day)} (yearly)`;
    if (ev.recurrence === 'monthly') return `day ${ev.day} (monthly)`;
    if (ev.recurrence === 'weekly')  return `weekday of ${d}`;
    if (ev.recurrence === 'daily')   return `daily`;
    return d;
}

function sortByNextOccurrence(a, b) {
    // Simple order: disabled last, rest by id (stable).
    if (a.enabled !== b.enabled) return a.enabled ? -1 : 1;
    return a.id.localeCompare(b.id);
}

// ─────────────────────────────────────────────────────────────────────────────
// Form
// ─────────────────────────────────────────────────────────────────────────────
function evFormReset() {
    editingId = null;
    document.getElementById('ev_form_title').textContent = '➕ New event';
    document.getElementById('ev_cancel_btn').style.display = 'none';
    document.getElementById('ev_type').value = 'reminder';
    document.getElementById('ev_title').value = '';
    document.getElementById('ev_date').value = todayStr();
    document.getElementById('ev_time').value = nowTimeStr();
    document.getElementById('ev_recurrence').value = 'none';
    document.getElementById('ev_enabled').checked = true;
    document.getElementById('ev_station').value = '0';
    document.getElementById('ev_volume').value = '50';
    document.getElementById('ev_volume_val').textContent = '50';
    document.getElementById('ev_sound').value = '';
    document.getElementById('ev_sound_play').style.display = 'none';
    resetClipAudio();
    evTypeChanged();
    setStatus('', '');
}

function evEdit(id) {
    const ev = events.find(e => e.id === id);
    if (!ev) return;

    editingId = id;
    document.getElementById('ev_form_title').textContent = '✎ Edit event';
    document.getElementById('ev_cancel_btn').style.display = '';

    document.getElementById('ev_type').value     = ev.type;
    document.getElementById('ev_title').value    = ev.title;
    document.getElementById('ev_date').value     = `${ev.year}-${pad2(ev.month)}-${pad2(ev.day)}`;
    document.getElementById('ev_time').value     = `${pad2(ev.hour)}:${pad2(ev.minute)}`;
    document.getElementById('ev_recurrence').value = ev.recurrence;
    document.getElementById('ev_enabled').checked = !!ev.enabled;
    document.getElementById('ev_station').value = String(ev.station ?? 0);
    const vol = ev.volume ?? 50;
    document.getElementById('ev_volume').value = String(vol);
    document.getElementById('ev_volume_val').textContent = String(vol);
    document.getElementById('ev_sound').value = ev.sound || '';
    resetClipAudio();
    document.getElementById('ev_sound_play').style.display =
        (ev.type === 'voice' && ev.sound) ? '' : 'none';
    evTypeChanged();

    window.scrollTo({ top: 0, behavior: 'smooth' });
}

function formToEvent() {
    const date = document.getElementById('ev_date').value;
    if (!date) { setStatus('Date required', 'error'); return null; }
    const [y, m, d] = date.split('-').map(Number);

    const t = document.getElementById('ev_time').value || '08:00';
    const [hour, minute] = t.split(':').map(Number);

    const title = document.getElementById('ev_title').value.trim();
    if (!title) { setStatus('Title required', 'error'); return null; }

    const type    = document.getElementById('ev_type').value;
    const station = parseInt(document.getElementById('ev_station').value, 10) || 0;
    let   volume  = parseInt(document.getElementById('ev_volume').value, 10);
    if (isNaN(volume)) volume = 50;
    if (volume < 0)   volume = 0;
    if (volume > 100) volume = 100;

    if (type === 'alarm' && stations.length === 0) {
        setStatus('Add at least one station to the playlist first', 'error');
        return null;
    }

    return {
        type,
        title,
        year:       y,
        month:      m,
        day:        d,
        hour,
        minute,
        recurrence: document.getElementById('ev_recurrence').value,
        enabled:    document.getElementById('ev_enabled').checked,
        station,
        volume,
        // Preserve the voice clip on edit; empty for non-voice events (ignored
        // by the firmware). The web UI can't record audio — only the app sets it.
        sound:      document.getElementById('ev_sound').value || '',
    };
}

async function evSave() {
    const payload = formToEvent();
    if (!payload) return;

    const btn = document.getElementById('ev_save_btn');
    btn.disabled = true;
    setStatus('Saving…', '');

    const url    = editingId ? `/api/events/${editingId}` : '/api/events';
    const method = editingId ? 'PUT' : 'POST';

    try {
        const res = await fetch(url, {
            method,
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload),
        });
        if (!res.ok) {
            const txt = await res.text().catch(() => '');
            throw new Error(`HTTP ${res.status} ${txt}`);
        }
        setStatus('✓ Saved', 'ok');
        evFormReset();
        await evLoad();
    } catch (e) {
        console.error(e);
        setStatus('Save failed: ' + e.message, 'error');
    } finally {
        btn.disabled = false;
    }
}

async function evDelete(id) {
    const ev = events.find(e => e.id === id);
    if (!ev) return;
    if (!confirm(`Delete "${ev.title}"?`)) return;

    try {
        const res = await fetch(`/api/events/${id}`, { method: 'DELETE' });
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        if (editingId === id) evFormReset();
        await evLoad();
    } catch (e) {
        console.error(e);
        setStatus('Delete failed: ' + e.message, 'error');
    }
}

async function evToggleEnabled(id, enabled) {
    try {
        const res = await fetch(`/api/events/${id}`, {
            method: 'PUT',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ enabled }),
        });
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        await evLoad();
    } catch (e) {
        console.error(e);
        setStatus('Toggle failed: ' + e.message, 'error');
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Utils
// ─────────────────────────────────────────────────────────────────────────────
function setStatus(text, cls) {
    const el = document.getElementById('ev_status');
    el.textContent = text;
    el.className = 'save-status' + (cls ? ' ' + cls : '');
}

function pad2(n) { return String(n).padStart(2, '0'); }

function todayStr() {
    const d = new Date();
    return `${d.getFullYear()}-${pad2(d.getMonth() + 1)}-${pad2(d.getDate())}`;
}

function nowTimeStr() {
    const d = new Date();
    return `${pad2(d.getHours())}:${pad2(d.getMinutes())}`;
}

function escapeHtml(s) {
    return String(s)
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;');
}

// ─────────────────────────────────────────────────────────────────────────────
// Init
// ─────────────────────────────────────────────────────────────────────────────
(async () => {
    const audio = document.getElementById('ev_sound_audio');
    const btn   = document.getElementById('ev_sound_play');
    audio.addEventListener('play',  () => btn.textContent = '⏹ Stop');
    audio.addEventListener('pause', () => btn.textContent = '▶ Play');
    audio.addEventListener('ended', () => btn.textContent = '▶ Play');

    await loadStations();
    evFormReset();
    await evLoad();
})();
