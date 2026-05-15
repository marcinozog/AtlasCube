'use strict';

// ─────────────────────────────────────────────────────────────────────────────
// State
// ─────────────────────────────────────────────────────────────────────────────
let ws;
let volTimeout, eqTimeout, btVolTimeout;
let currentStation      = null;
let currentStationIndex = null;
let stationsList        = [];
let btEnabled           = false;
let btState             = -1;

const codecMap = {
    0:"UNK", 1:"RAW", 2:"WAV", 3:"MP3", 4:"AAC", 5:"OPUS",
    6:"M4A", 7:"MP4", 8:"FLAC", 9:"OGG", 10:"TSAAC",
    11:"AMR-NB", 12:"AMR-WB", 13:"PCM", 14:"M3U8", 15:"PLS", 16:"UNSUP"
};

const freqs = ["31","62","125","250","500","1k","2k","4k","8k","16k"];

const btStateMap = {
    "BT_CONNECTED":0, "BT_DISCONNECTED":1, "BT_DISCOVERABLE":2
}

// ─────────────────────────────────────────────────────────────────────────────
// WebSocket
// ─────────────────────────────────────────────────────────────────────────────
function connect() {
    ws = new WebSocket(`ws://${location.host}/ws`);

    ws.onopen  = () => console.log('WS connected');
    ws.onclose = () => { console.log('WS disconnected'); setTimeout(connect, 2000); };
    ws.onerror = () => console.warn('WS error');

    ws.onmessage = (msg) => {
        console.log('RX:', msg.data);
        let data;
        try { data = JSON.parse(msg.data); } catch { return; }

        if (data.type === 'state') {
            updateRadioStatus(data.radio);
            setVolumeUI(data.volume);

            if (data.curr_index !== undefined) {
                currentStationIndex = data.curr_index;
                selectStation(currentStationIndex);
            }
            if (data.station_name?.length) {
                document.getElementById('station_name').innerText = data.station_name;
                if (!data.title?.length)
                    document.getElementById('title').innerText = data.station_name;
            }
            if (data.title !== undefined) {
                const titleEl = document.getElementById('title');
                if (data.title?.length) titleEl.innerText = data.title;
                else if (currentStation)  titleEl.innerText = currentStation.name;
                else                      titleEl.innerText = '---';
            }
            if (data.sr !== undefined) {
                const infoEl = document.getElementById('audio_info');
                if (data.sr > 0) {
                    const codec = codecMap[data.fmt] || 'UNK';
                    const sr = data.sr >= 1000 ? (data.sr / 1000) + 'kHz' : data.sr + 'Hz';
                    const ch = data.ch === 2 ? 'stereo' : (data.ch === 1 ? 'mono' : data.ch + 'ch');
                    let txt = `${codec} · ${sr} · ${data.bits}bit · ${ch}`;
                    if (data.br) txt += ` · ${Math.round(data.br / 1000)} kbps`;
                    infoEl.innerText = txt;
                } else {
                    infoEl.innerText = '---';
                }
            }
            if (data.eq !== undefined) setEqUI(data.eq);
            if (data.bt_enable !== undefined) {
                btEnabled = data.bt_enable;
                updateBtButton();
            }
            if (data.bt_volume !== undefined) {
                const sl = document.getElementById('bt_volume_slider');
                const lb = document.getElementById('bt_vol_value');
                if (sl) sl.value = data.bt_volume;
                if (lb) lb.innerText = data.bt_volume;
            }
            if (data.bt_state !== undefined) {
                btState= data.bt_state;
                updateBtStatus();
            }
            if (data.bt_title !== undefined) {
                const el = document.getElementById('bt_title');
                if (el) el.innerText = data.bt_title?.length ? data.bt_title : '---';
            }
            if (data.bt_artist !== undefined) {
                const el = document.getElementById('bt_artist');
                if (el) el.innerText = data.bt_artist?.length ? data.bt_artist : '---';
            }
            if (data.bt_position_s !== undefined || data.bt_duration_ms !== undefined) {
                const el = document.getElementById('bt_time');
                if (el) el.innerText = formatBtTime(data.bt_position_s, data.bt_duration_ms);
            }
        }

        if (data.type === 'bt_log') {
            const el = document.getElementById('bt_log');
            if (el) { el.innerText += data.data + '\n'; el.scrollTop = el.scrollHeight; }
        }
    };
}

function send(obj) {
    if (ws && ws.readyState === WebSocket.OPEN) ws.send(JSON.stringify(obj));
    else console.warn('WS not connected');
}

function formatBtTime(posS, durMs) {
    const fmt = (s) => {
        s = Math.max(0, Math.floor(s || 0));
        return `${Math.floor(s / 60)}:${String(s % 60).padStart(2, '0')}`;
    };
    return `${fmt(posS)} / ${fmt((durMs || 0) / 1000)}`;
}

// ─────────────────────────────────────────────────────────────────────────────
// Playback
// ─────────────────────────────────────────────────────────────────────────────
function playCurrent() {
    if (currentStationIndex === null) return;
    send({ cmd: 'play_index', index: currentStationIndex });
}

function stop() {
    send({ cmd: 'stop' });
}

// ─────────────────────────────────────────────────────────────────────────────
// Status
// ─────────────────────────────────────────────────────────────────────────────
function updateRadioStatus(state) {
    const badge = document.getElementById('status');
    const playBtn = document.getElementById('play_btn');

    const states = {
        playing:   { text: '▶ Playing',   cls: 'playing' },
        stopped:   { text: '⏹ Stopped',   cls: '' },
        buffering: { text: '⏳ Buffering', cls: 'buffering' },
        error:     { text: '❌ Error',     cls: 'error' },
    };

    const s = states[state] || { text: '? Unknown', cls: '' };
    badge.innerText = s.text;
    badge.className = 'status-badge' + (s.cls ? ' ' + s.cls : '');

    if (state === 'playing') {
        playBtn.innerText = '⏹ Stop';
        playBtn.onclick   = stop;
    } else {
        playBtn.innerText = '▶ Play';
        playBtn.onclick   = playCurrent;
    }

    if (state === 'stopped') document.getElementById('audio_info').innerText = '---';
}

// ─────────────────────────────────────────────────────────────────────────────
// Volume
// ─────────────────────────────────────────────────────────────────────────────
function onVolumeChange(v) {
    document.getElementById('vol_value').innerText = v;
    clearTimeout(volTimeout);
    volTimeout = setTimeout(() => send({ cmd: 'set_volume', value: parseInt(v) }), 150);
}

function setVolumeUI(v) {
    document.getElementById('volume_slider').value = v;
    document.getElementById('vol_value').innerText = v;
}

// ─────────────────────────────────────────────────────────────────────────────
// Playlist / stations
// ─────────────────────────────────────────────────────────────────────────────
async function loadPlaylist() {
    try {
        const res  = await fetch('/data/playlist.csv');
        const text = await res.text();
        const lines = text.trim().split(/\r?\n/);
        stationsList = lines.map(line => {
            const [name, url, selected] = line.split('\t');
            return { name, url, selected: selected === '1' };
        });
        renderStations(stationsList);
    } catch (e) {
        console.error('Playlist load error', e);
    }
}

function selectStation(index) {
    if (!stationsList?.[index]) return;
    currentStation = stationsList[index];
    document.getElementById('station_name').innerText = currentStation.name;
    document.getElementById('title').innerText        = currentStation.name;
    document.querySelectorAll('.station').forEach(e => e.classList.remove('active'));
    const el = document.getElementById('stations').children[index];
    if (el) { el.classList.add('active'); el.scrollIntoView({ behavior: 'auto', block: 'nearest' }); }
}

function renderStations(stations) {
    const container = document.getElementById('stations');
    container.innerHTML = '';
    stations.forEach((st, index) => {
        const el = document.createElement('div');
        el.className  = 'station';
        el.innerText  = st.name;
        if (currentStationIndex === index) {
            el.classList.add('active');
            currentStation = st;
            document.getElementById('station_name').innerText = st.name;
            document.getElementById('title').innerText        = st.name;
        }
        el.onclick = () => {
            selectStation(index);
            send({ cmd: 'play_index', index });
        };
        container.appendChild(el);
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// Equalizer
// ─────────────────────────────────────────────────────────────────────────────
function renderEQ() {
    const container = document.getElementById('eq_bands');
    container.innerHTML = '';
    freqs.forEach((f, i) => {
        const el = document.createElement('div');
        el.innerHTML = `
            <label>${f}</label>
            <input type="range" min="-13" max="13" value="0" id="eq_${i}" oninput="onEqBandChange()" />
        `;
        container.appendChild(el);
    });
}

function onEqBandChange() {
    clearTimeout(eqTimeout);
    eqTimeout = setTimeout(() => {
        const values = freqs.map((_, i) => parseInt(document.getElementById(`eq_${i}`).value));
        drawEQ(values);
        send({ cmd: 'set_eq_10', bands: values });
    }, 150);
}

function setEqUI(eq) {
    eq.forEach((v, i) => { const s = document.getElementById(`eq_${i}`); if (s) s.value = v; });
    drawEQ(eq);
}

function drawEQ(values) {
    const canvas = document.getElementById('eq_canvas');
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    const W = canvas.width, H = canvas.height;
    ctx.clearRect(0, 0, W, H);
    ctx.fillStyle = '#0a0a0e'; ctx.fillRect(0, 0, W, H);
    ctx.strokeStyle = '#1e1e28'; ctx.lineWidth = 1;
    [.25, .5, .75].forEach(r => {
        ctx.beginPath(); ctx.moveTo(0, H * r); ctx.lineTo(W, H * r); ctx.stroke();
    });
    ctx.strokeStyle = '#00aaff'; ctx.lineWidth = 2;
    ctx.beginPath();
    const step = W / (values.length - 1);
    values.forEach((v, i) => {
        const x = i * step, y = H / 2 - v * (H / 28);
        if (i === 0) { ctx.moveTo(x, y); return; }
        const px = (i - 1) * step, py = H / 2 - values[i-1] * (H / 28);
        ctx.quadraticCurveTo(px, py, (px + x) / 2, (py + y) / 2);
    });
    ctx.stroke();
}

function resetEQ() {
    const values = new Array(freqs.length).fill(0);
    setEqUI(values);
    send({ cmd: 'set_eq_10', bands: values });
}

function openEq()  { document.getElementById('eq_modal').classList.remove('hidden'); }
function closeEq() { document.getElementById('eq_modal').classList.add('hidden'); }

// ─────────────────────────────────────────────────────────────────────────────
// Bluetooth
// ─────────────────────────────────────────────────────────────────────────────
function toggleBtMode() {
    btEnabled = !btEnabled;
    send({ cmd: 'bt_enable', value: btEnabled });
    updateBtButton();
}

function updateBtStatus() {
    const el = document.getElementById('bt_status');
    if (!el) return;
    if (btState == btStateMap["BT_CONNECTED"]) {
        el.textContent = '⬤ connected';
        el.className = 'bt-status connected';
    } else if ((btState == btStateMap["BT_DISCONNECTED"])) {
        el.textContent = '⬤ disconnected';
        el.className = 'bt-status disconnected';
    }
    else if ((btState == btStateMap["BT_DISCOVERABLE"])) {
        el.textContent = '⬤ discoverable';
        el.className = 'bt-status discoverable';
    }
}

function updateBtButton() {
    const label = btEnabled ? '🔵 BT ON' : '⚪ BT OFF';

    // toolbar button - now bt_console
    // const btn = document.getElementById('bt_btn');
    // if (btn) {
    //     btn.innerText = label;
    //     btn.classList.toggle('bt-on', btEnabled);
    // }

    // modal toggle button
    const toggleBtn = document.getElementById('bt_toggle_btn');
    if (toggleBtn) {
        toggleBtn.innerText = label;
        toggleBtn.classList.toggle('bt-on', btEnabled);
    }
}

function onBtVolumeChange(v) {
    document.getElementById('bt_vol_value').innerText = v;
    clearTimeout(btVolTimeout);
    btVolTimeout = setTimeout(() => send({ cmd: 'bt_volume', value: parseInt(v) }), 150);
}

function sendBt() {
    const v = document.getElementById('bt_input').value;
    send({ cmd: 'bt_cmd', value: v });
}

function openBtConsole()  { document.getElementById('bt_modal').classList.remove('hidden'); }
function closeBtConsole() { document.getElementById('bt_modal').classList.add('hidden'); }

// ─────────────────────────────────────────────────────────────────────────────
// Screens
// ─────────────────────────────────────────────────────────────────────────────
function setScreen(name) {
    send({ cmd: 'set_screen', value: name });
}

// ─────────────────────────────────────────────────────────────────────────────
// Modal helpers
// ─────────────────────────────────────────────────────────────────────────────
function onModalBgClick(e, id) {
    if (e.target === document.getElementById(id)) {
        document.getElementById(id).classList.add('hidden');
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Init
// ─────────────────────────────────────────────────────────────────────────────
connect();
loadPlaylist();
renderEQ();