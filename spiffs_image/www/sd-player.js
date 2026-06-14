// AtlasCube — SD music player UI with folder browsing. One persistent WebSocket
// (no per-command reconnects, which fragment the device's internal RAM).

let ws = null;
let curDir = '';       // currently browsed folder
let parentDir = '';    // parent of curDir ('' at the root)
let folders = [];      // subfolders of curDir
let tracks = [];       // audio files in curDir
let playDir = '';      // folder of the playing track (from state)
let playTrack = '';    // playing track file name (from state)
let active = false;
let paused = false;
let shuffleOn = false;
let repeatMode = 0;    // 0 none, 1 all, 2 one
let volTimeout = null;

function connect() {
    ws = new WebSocket(`ws://${location.host}/ws`);

    ws.onopen  = () => { console.log('WS connected'); send({ cmd: 'sd_list' }); };
    ws.onclose = () => { console.log('WS disconnected'); setTimeout(connect, 2000); };
    ws.onerror = () => console.warn('WS error');

    ws.onmessage = (msg) => {
        let d;
        try { d = JSON.parse(msg.data); } catch { return; }

        if (d.type === 'sd_list') {
            curDir    = d.dir || '';
            parentDir = d.parent || '';
            folders   = d.folders || [];
            tracks    = d.tracks || [];
            renderList();
        } else if (d.type === 'state') {
            applyState(d);
        }
    };
}

function send(obj) {
    if (ws && ws.readyState === WebSocket.OPEN) ws.send(JSON.stringify(obj));
    else console.warn('WS not connected');
}

function joinPath(dir, name) {
    return dir.replace(/\/+$/, '') + '/' + name;
}

function applyState(d) {
    if (d.sd_active  !== undefined) active = !!d.sd_active;
    if (d.sd_dir     !== undefined) playDir = d.sd_dir;
    if (d.sd_track   !== undefined) playTrack = d.sd_track;
    if (d.sd_paused  !== undefined) paused = !!d.sd_paused;
    if (d.sd_shuffle !== undefined) shuffleOn = !!d.sd_shuffle;
    if (d.sd_repeat  !== undefined) repeatMode = d.sd_repeat | 0;

    const trackEl = document.getElementById('npTrack');
    const idxEl   = document.getElementById('npIndex');
    const infoEl  = document.getElementById('npInfo');

    if (active) {
        trackEl.innerText = d.sd_track || playTrack || '---';
        const pos = (d.sd_count ? `${(d.sd_index ?? 0) + 1} / ${d.sd_count} · ` : '');
        idxEl.innerText = pos + (paused ? '⏸ pauza' : '▶ gra');
    } else {
        trackEl.innerText = '---';
        idxEl.innerText = '⏹ zatrzymane';
    }

    updateModeButtons();

    if (d.volume !== undefined) setVolumeUI(d.volume);

    if (d.sr !== undefined) {
        if (active && d.sr > 0) {
            const sr = d.sr >= 1000 ? (d.sr / 1000) + 'kHz' : d.sr + 'Hz';
            const ch = d.ch === 2 ? 'stereo' : (d.ch === 1 ? 'mono' : d.ch + 'ch');
            let t = `${sr} · ${d.bits}bit · ${ch}`;
            if (d.br) t += ` · ${Math.round(d.br / 1000)} kbps`;
            infoEl.innerText = t;
        } else {
            infoEl.innerText = '---';
        }
    }

    highlight();
}

function makeItem(icon, label, onclick, isTrack) {
    const li = document.createElement('li');
    li.className = isTrack ? 'track-item' : 'folder-item';
    li.innerHTML = `<span class="num">${icon}</span><span>${escapeHtml(label)}</span>`;
    li.onclick = onclick;
    return li;
}

function renderList() {
    const ul = document.getElementById('trackList');
    document.getElementById('dirLabel').innerText = curDir || '/sdcard/music';
    ul.innerHTML = '';

    if (parentDir) ul.appendChild(makeItem('⬆', '..', () => browse(parentDir), false));

    folders.forEach(name =>
        ul.appendChild(makeItem('📁', name, () => browse(joinPath(curDir, name)), false)));

    tracks.forEach(name => {
        const li = makeItem('🎵', name, () => playPath(joinPath(curDir, name)), true);
        li.dataset.name = name;
        ul.appendChild(li);
    });

    if (!folders.length && !tracks.length) {
        const li = document.createElement('li');
        li.className = 'empty';
        li.innerText = 'Brak plików audio';
        ul.appendChild(li);
    }

    highlight();
}

function highlight() {
    const ul = document.getElementById('trackList');
    [...ul.querySelectorAll('.track-item')].forEach(li =>
        li.classList.toggle('active',
            active && curDir === playDir && li.dataset.name === playTrack));
}

function escapeHtml(s) {
    return s.replace(/[&<>"]/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
}

// Volume — shared with the radio/BT output (same set_volume command). Debounced
// like the main page so dragging doesn't flood the socket.
function onVolumeChange(v) {
    document.getElementById('vol_value').innerText = v;
    clearTimeout(volTimeout);
    volTimeout = setTimeout(() => send({ cmd: 'set_volume', value: parseInt(v) }), 150);
}

function setVolumeUI(v) {
    document.getElementById('volume_slider').value = v;
    document.getElementById('vol_value').innerText = v;
}

function updateModeButtons() {
    const pp = document.getElementById('ppBtn');
    if (pp) pp.innerText = (active && !paused) ? '⏸' : '▶';

    const sh = document.getElementById('shufBtn');
    if (sh) sh.classList.toggle('on', shuffleOn);

    const rp = document.getElementById('repBtn');
    if (rp) {
        rp.innerText = repeatMode === 2 ? '🔂' : '🔁';
        rp.classList.toggle('on', repeatMode !== 0);
    }
}

function browse(dir)    { send({ cmd: 'sd_list', dir }); }
function playPath(path) { send({ cmd: 'sd_play_path', path }); }
function scan()         { send({ cmd: 'sd_list', dir: curDir || undefined }); }
function playAll()      { send({ cmd: 'sd_play', dir: curDir || undefined }); }
function playPause()    { active ? send({ cmd: 'sd_pause' }) : playAll(); }
function next()         { send({ cmd: 'sd_next' }); }
function prev()         { send({ cmd: 'sd_prev' }); }
function stop()         { send({ cmd: 'sd_stop' }); }
function shuffle()      { send({ cmd: 'sd_shuffle' }); }
function repeat()       { send({ cmd: 'sd_repeat' }); }

connect();
