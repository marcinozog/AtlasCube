'use strict';

// ─────────────────────────────────────────────────────────────────────────────
// AtlasCube — Bluetooth panel
// ─────────────────────────────────────────────────────────────────────────────
let ws;
let btVolTimeout;
let btEnabled = false;
let btState   = -1;
let btLogAutoScroll = true;

const btStateMap = {
    "BT_CONNECTED":0, "BT_DISCONNECTED":1, "BT_DISCOVERABLE":2
};

// ─────────────────────────────────────────────────────────────────────────────
// WebSocket
// ─────────────────────────────────────────────────────────────────────────────
function connect() {
    ws = new WebSocket(`ws://${location.host}/ws`);

    ws.onopen  = () => console.log('WS connected');
    ws.onclose = () => { console.log('WS disconnected'); setTimeout(connect, 2000); };
    ws.onerror = () => console.warn('WS error');

    ws.onmessage = (msg) => {
        let data;
        try { data = JSON.parse(msg.data); } catch { return; }

        if (data.type === 'state') {
            if (data.bt_enable !== undefined) {
                btEnabled = data.bt_enable;
                updateAudioSource(btEnabled);
            }
            if (data.bt_volume !== undefined) {
                const sl = document.getElementById('bt_volume_slider');
                const lb = document.getElementById('bt_vol_value');
                if (sl) sl.value = data.bt_volume;
                if (lb) lb.innerText = data.bt_volume;
            }
            if (data.bt_state !== undefined) {
                btState = data.bt_state;
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
            appendBtLog(data.data);
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
// Bluetooth
// ─────────────────────────────────────────────────────────────────────────────
// Audio source (Radio / Bluetooth) — drives bluetooth.enable
function setAudioSource(bt) {
    btEnabled = bt;
    updateAudioSource(btEnabled);
    send({ cmd: 'bt_enable', value: btEnabled });
}

function updateBtStatus() {
    const el = document.getElementById('bt_status');
    if (!el) return;
    if (btState == btStateMap["BT_CONNECTED"]) {
        el.textContent = '⬤ connected';
        el.className = 'bt-status connected';
    } else if (btState == btStateMap["BT_DISCONNECTED"]) {
        el.textContent = '⬤ disconnected';
        el.className = 'bt-status disconnected';
    } else if (btState == btStateMap["BT_DISCOVERABLE"]) {
        el.textContent = '⬤ discoverable';
        el.className = 'bt-status discoverable';
    }
}

function updateAudioSource(bt) {
    document.getElementById('srcRadioBtn')?.classList.toggle('active', !bt);
    document.getElementById('srcBtBtn')   ?.classList.toggle('active',  bt);
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

function btCtrl(at) {
    send({ cmd: 'bt_cmd', value: at });
}

// Append a line, auto-scrolling only while the user is following the bottom.
function appendBtLog(line) {
    const el = document.getElementById('bt_log');
    if (!el) return;
    el.innerText += line + '\n';
    if (btLogAutoScroll) el.scrollTop = el.scrollHeight;
}

function clearBtLog() {
    const el = document.getElementById('bt_log');
    if (el) el.innerText = '';
    btLogAutoScroll = true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Init
// ─────────────────────────────────────────────────────────────────────────────
connect();

const logEl = document.getElementById('bt_log');
if (logEl) {
    // Scrolling up pauses auto-scroll; returning to the bottom resumes it.
    logEl.addEventListener('scroll', () => {
        btLogAutoScroll = logEl.scrollHeight - logEl.scrollTop - logEl.clientHeight < 8;
    });
}
