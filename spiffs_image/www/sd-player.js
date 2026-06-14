// AtlasCube — SD music player UI. One persistent WebSocket (no per-command
// reconnects, which fragment the device's internal RAM).

let ws = null;
let tracks = [];
let curIndex = -1;
let active = false;

function connect() {
    ws = new WebSocket(`ws://${location.host}/ws`);

    ws.onopen  = () => { console.log('WS connected'); send({ cmd: 'sd_list' }); };
    ws.onclose = () => { console.log('WS disconnected'); setTimeout(connect, 2000); };
    ws.onerror = () => console.warn('WS error');

    ws.onmessage = (msg) => {
        let d;
        try { d = JSON.parse(msg.data); } catch { return; }

        if (d.type === 'sd_list')      { tracks = d.tracks || []; renderList(); }
        else if (d.type === 'state')   { applyState(d); }
    };
}

function send(obj) {
    if (ws && ws.readyState === WebSocket.OPEN) ws.send(JSON.stringify(obj));
    else console.warn('WS not connected');
}

function applyState(d) {
    if (d.sd_active !== undefined) active = !!d.sd_active;
    if (d.sd_index  !== undefined) curIndex = d.sd_index;

    const trackEl = document.getElementById('npTrack');
    const idxEl   = document.getElementById('npIndex');
    const infoEl  = document.getElementById('npInfo');

    if (active && d.sd_track) {
        trackEl.innerText = d.sd_track;
        const pos = (d.sd_count ? `${(d.sd_index ?? 0) + 1} / ${d.sd_count} · ` : '');
        idxEl.innerText = pos + '▶ gra';
    } else if (!active) {
        trackEl.innerText = '---';
        idxEl.innerText = '⏹ zatrzymane';
    }

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

function renderList() {
    const ul = document.getElementById('trackList');
    if (!tracks.length) {
        ul.innerHTML = '<li class="empty">Brak plików audio w /sdcard/music</li>';
        return;
    }
    ul.innerHTML = '';
    tracks.forEach((name, i) => {
        const li = document.createElement('li');
        li.innerHTML = `<span class="num">${i + 1}</span><span>${escapeHtml(name)}</span>`;
        li.onclick = () => send({ cmd: 'sd_play_index', index: i });
        ul.appendChild(li);
    });
    highlight();
}

function highlight() {
    const ul = document.getElementById('trackList');
    [...ul.children].forEach((li, i) =>
        li.classList.toggle('active', active && i === curIndex));
}

function escapeHtml(s) {
    return s.replace(/[&<>"]/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
}

function scan()    { send({ cmd: 'sd_list' }); }
function playAll() { send({ cmd: 'sd_play' }); }
function next()    { send({ cmd: 'sd_next' }); }
function prev()    { send({ cmd: 'sd_prev' }); }
function stop()    { send({ cmd: 'sd_stop' }); }

connect();
