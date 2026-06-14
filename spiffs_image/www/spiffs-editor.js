/* AtlasCube — on-device file editor for /spiffs files served by http_server.c
   GET /api/files            → list
   GET /<name>               → file content (gzip is transparently decoded by browser)
   PUT /api/files/<name>     → save (server gzips html/css/js, stores json/csv raw) */

let currentName = null;
let currentIsGz = false;

const $sel  = document.getElementById('file_select');
const $code = document.getElementById('code');
const $meta = document.getElementById('file_meta');
const $st   = document.getElementById('status');
const $js   = document.getElementById('json_status');

function isJsonFile(name) {
    return !!name && name.toLowerCase().endsWith('.json');
}

function setJsonStatus(kind, msg) {
    if (!kind) { $js.className = 'json-status hidden'; $js.textContent = ''; return; }
    $js.className = 'json-status ' + kind;
    $js.textContent = msg;
}

/* Parse error position → 1-based line/col for the user. */
function jsonErrorLocation(text, err) {
    const m = /position\s+(\d+)/i.exec(err && err.message || '');
    if (!m) return null;
    const pos = Math.min(parseInt(m[1], 10), text.length);
    let line = 1, col = 1;
    for (let i = 0; i < pos; i++) {
        if (text.charCodeAt(i) === 10) { line++; col = 1; } else { col++; }
    }
    return { line, col };
}

function validateJson(text) {
    if (!text.trim()) { setJsonStatus('ok', '✓ empty'); return; }
    try {
        JSON.parse(text);
        setJsonStatus('ok', '✓ valid JSON');
    } catch (e) {
        const loc = jsonErrorLocation(text, e);
        setJsonStatus('err', '✗ ' + (loc ? 'line ' + loc.line + ', col ' + loc.col + ': ' : '') + e.message);
    }
}

function setStatus(msg, kind) {
    $st.className = 'status' + (kind ? ' ' + kind : '');
    $st.textContent = msg;
}

function fmtSize(n) {
    if (n < 1024) return n + ' B';
    return (n / 1024).toFixed(1) + ' KB';
}

function encodePath(p) {
    return p.split('/').map(encodeURIComponent).join('/');
}

async function loadFileList(keepSelection) {
    setStatus('Loading file list…');
    try {
        const r = await fetch('/api/files', { cache: 'no-store' });
        if (!r.ok) throw new Error('HTTP ' + r.status);
        const list = await r.json();
        list.sort((a, b) => a.name.localeCompare(b.name));

        const prev = keepSelection ? currentName : null;
        $sel.innerHTML = '';
        for (const f of list) {
            const opt = document.createElement('option');
            opt.value = f.name;
            opt.dataset.gz = f.gz ? '1' : '0';
            opt.dataset.size = f.size;
            opt.textContent = f.name + '  (' + (f.gz ? 'gz ' : '') + fmtSize(f.size) + ')';
            $sel.appendChild(opt);
        }
        if (!list.length) {
            setStatus('No editable files in /spiffs', 'warn');
            return;
        }

        let idx = 0;
        if (prev) {
            for (let i = 0; i < $sel.options.length; i++) {
                if ($sel.options[i].value === prev) { idx = i; break; }
            }
        }
        $sel.selectedIndex = idx;
        await loadFile();
    } catch (e) {
        setStatus('Failed to list files: ' + e.message, 'err');
    }
}

async function loadFile() {
    const opt = $sel.options[$sel.selectedIndex];
    if (!opt) return;
    currentName = opt.value;
    currentIsGz = opt.dataset.gz === '1';

    setStatus('Loading ' + currentName + '…');
    try {
        // GET via normal file route — browser transparently decompresses gzip
        const r = await fetch('/' + encodePath(currentName), { cache: 'no-store' });
        if (!r.ok) throw new Error('HTTP ' + r.status);
        let text = await r.text();

        let prettyNote = '';
        if (isJsonFile(currentName) && text.trim()) {
            try {
                text = JSON.stringify(JSON.parse(text), null, 2);
                prettyNote = ' (pretty-printed)';
            } catch (_) { /* leave raw so user can fix syntax */ }
        }

        $code.value = text;
        $code.scrollTop = 0;
        $meta.textContent =
            (currentIsGz ? 'gz · ' : 'raw · ') +
            fmtSize(opt.dataset.size) + ' flash · ' +
            text.length + ' chars';
        setStatus('Loaded ' + currentName + prettyNote, 'ok');

        if (isJsonFile(currentName)) { validateJson(text); } else { setJsonStatus(null); }
    } catch (e) {
        setStatus('Load failed: ' + e.message, 'err');
    }
}

async function reloadFile() {
    await loadFileList(true);
}

async function saveFile() {
    if (!currentName) return;
    setStatus('Saving ' + currentName + '…');
    try {
        const r = await fetch('/api/files/' + encodePath(currentName), {
            method: 'PUT',
            headers: { 'Content-Type': 'text/plain; charset=utf-8' },
            body: $code.value
        });
        if (!r.ok) {
            const err = await r.text().catch(() => '');
            throw new Error('HTTP ' + r.status + (err ? ' — ' + err : ''));
        }
        const info = await r.json().catch(() => null);
        setStatus(
            'Saved ' + currentName +
            (info ? ' (' + (info.gz ? 'gz ' : '') + fmtSize(info.size) + ')' : ''),
            'ok');
        await loadFileList(true);
    } catch (e) {
        setStatus('Save failed: ' + e.message, 'err');
    }
}

function downloadFile() {
    if (!currentName) return;
    const blob = new Blob([$code.value], { type: 'text/plain;charset=utf-8' });
    const url  = URL.createObjectURL(blob);
    const a    = document.createElement('a');
    a.href     = url;
    a.download = currentName.split('/').pop();
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
}

$sel.addEventListener('change', loadFile);

let _jsonValidateTimer = null;
$code.addEventListener('input', () => {
    if (!isJsonFile(currentName)) return;
    clearTimeout(_jsonValidateTimer);
    _jsonValidateTimer = setTimeout(() => validateJson($code.value), 200);
});

$code.addEventListener('keydown', (e) => {
    if (e.key === 'Tab') {
        e.preventDefault();
        const s = $code.selectionStart;
        const v = $code.value;
        $code.value = v.substring(0, s) + '    ' + v.substring($code.selectionEnd);
        $code.selectionStart = $code.selectionEnd = s + 4;
    }
    if (e.key.toLowerCase() === 's' && (e.ctrlKey || e.metaKey)) {
        e.preventDefault();
        saveFile();
    }
});

loadFileList(false);
