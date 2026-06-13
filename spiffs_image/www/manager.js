// AtlasCube — dual-pane file manager.
// Left pane  = SPIFFS (flat, gzipped on flash)  via /api/files + GET /<name>
// Right pane = SD card (folders)                via /api/sd/*
// Transfer is copy-only and runs entirely in the browser: it reads from one
// side and writes to the other through the existing endpoints. No firmware
// changes — see http_server.c api_files_* / api_sd_* handlers.

// ── shared helpers ──────────────────────────────────────────────────────────
function fmtSize(n) {
    if (n < 1024) return n + " B";
    if (n < 1024 * 1024) return (n / 1024).toFixed(1) + " KB";
    return (n / (1024 * 1024)).toFixed(2) + " MB";
}

function esc(s) {
    return s.replace(/[&<>"']/g, c => (
        { "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]
    ));
}

function encodePath(p) {
    return p.split("/").map(encodeURIComponent).join("/");
}

// Extensions the inline editor will open as text. Anything else (images,
// .bin, .wav, .ico) gets no ✎ button.
const TEXT_EXTS = [".html", ".htm", ".css", ".js", ".json", ".csv",
                   ".txt", ".md", ".xml", ".svg", ".ini", ".conf"];
function fileExt(name) {
    const i = name.lastIndexOf(".");
    return i >= 0 ? name.slice(i).toLowerCase() : "";
}
function isEditable(name) { return TEXT_EXTS.includes(fileExt(name)); }
function isJson(name) { return fileExt(name) === ".json"; }

// Embed a string as a JS argument inside a single-quoted onclick attribute:
// JSON-encode (handles JS-level quoting) then HTML-escape (handles the
// attribute), so the parser hands JS a clean string literal.
function jsArg(s) { return esc(JSON.stringify(s)); }

const logEl = document.getElementById("log");
function setLog(msg, kind) {
    logEl.className = "mgr-log" + (kind ? " " + kind : "");
    logEl.textContent = msg;
}

// PUT /api/files only accepts text and rejects these; mirror that here so the
// user gets a clear reason instead of a silent 400 when restoring to SPIFFS.
const SPIFFS_MAX = 65536;
function spiffsRejectReason(name, size) {
    const ext = (name.lastIndexOf(".") >= 0 ? name.slice(name.lastIndexOf(".")) : "").toLowerCase();
    if (!ext) return "no file extension";
    if (ext === ".ico") return "binary (.ico) not supported";
    if (size > SPIFFS_MAX) return "too large (> " + fmtSize(SPIFFS_MAX) + ")";
    return null;
}

// ── SPIFFS pane (left) ──────────────────────────────────────────────────────
let spList = [];                                    // last listing, for re-render
const spListEl = document.getElementById("sp_listing");
const spMetaEl = document.getElementById("sp_meta");

function spShow(msg, isErr) {
    spListEl.innerHTML = `<div class="pane-status${isErr ? " err" : ""}">${esc(msg)}</div>`;
}

async function loadSpiffs() {
    spShow("Loading…");
    try {
        const r = await fetch("/api/files", { cache: "no-store" });
        if (!r.ok) throw new Error("HTTP " + r.status);
        spList = await r.json();
        spList.sort((a, b) => a.name.localeCompare(b.name));
        renderSpiffs();
    } catch (e) {
        spShow("Failed to list SPIFFS: " + e.message, true);
        spMetaEl.textContent = "error";
    }
}

function renderSpiffs() {
    if (!spList.length) { spShow("No editable files."); spMetaEl.textContent = "empty"; return; }

    let totBytes = 0;
    const rows = spList.map(f => {
        totBytes += f.size || 0;
        return `<tr class="row">` +
            `<td class="name"><span class="icon">📄</span>${esc(f.name)}` +
            (f.gz ? `<span class="badge">gz</span>` : "") + `</td>` +
            `<td class="size">${fmtSize(f.size || 0)}</td>` +
            `<td class="actions">` +
            `<button class="act copy" onclick='copyToSd(${jsArg(f.name)})' title="Copy to SD →">⮕</button>` +
            `<button class="act" onclick='openEditor("spiffs",${jsArg(f.name)})' title="Edit">✎</button>` +
            `</td></tr>`;
    });
    spListEl.innerHTML = `<table class="files"><tbody>${rows.join("")}</tbody></table>`;
    spMetaEl.textContent = `${spList.length} file${spList.length === 1 ? "" : "s"} · ${fmtSize(totBytes)}`;
}

// ── SD pane (right) ─────────────────────────────────────────────────────────
let sdPath = "/";
const sdListEl   = document.getElementById("sd_listing");
const sdCrumbsEl = document.getElementById("sd_crumbs");
const sdMetaEl   = document.getElementById("sd_meta");
const sdBarEl    = document.getElementById("sd_bar");

function sdJoin(dir, name) { return dir.endsWith("/") ? dir + name : dir + "/" + name; }
function sdParent(dir) {
    const p = dir.replace(/\/+$/, "");
    const i = p.lastIndexOf("/");
    return i <= 0 ? "/" : p.slice(0, i);
}

function sdShow(msg, isErr) {
    sdListEl.innerHTML = `<div class="pane-status${isErr ? " err" : ""}">${esc(msg)}</div>`;
}

function sdRenderCrumbs() {
    const parts = sdPath.split("/").filter(Boolean);
    let html = `<a onclick="sdNavTo('/')">SD</a>`;
    let acc = "";
    for (const part of parts) {
        acc += "/" + part;
        html += `<span class="sep">/</span><a onclick="sdNavTo('${esc(acc)}')">${esc(part)}</a>`;
    }
    sdCrumbsEl.innerHTML = html;
}

function sdNavTo(path) {
    sdPath = path || "/";
    sdRefresh();
}

async function sdRefresh() {
    sdRenderCrumbs();
    sdShow("Loading…");
    try {
        const r = await fetch("/api/sd/list?path=" + encodeURIComponent(sdPath));
        if (r.status === 503) { sdShow("No SD card inserted.", true); sdMetaEl.textContent = "no card"; return; }
        if (!r.ok) { sdShow("Error: " + r.status, true); return; }
        const data = await r.json();
        sdRender(data.entries || []);
    } catch (e) {
        sdShow("Connection error.", true);
    }
}

function sdRender(entries) {
    entries.sort((a, b) => {
        if (a.dir !== b.dir) return a.dir ? -1 : 1;
        return a.name.localeCompare(b.name, undefined, { numeric: true });
    });

    const rows = [];
    if (sdPath !== "/" && sdPath !== "") {
        rows.push(
            `<tr class="row"><td class="name dir" onclick="sdNavTo('${esc(sdParent(sdPath))}')">` +
            `<span class="icon">📁</span>..</td><td class="size"></td><td class="actions"></td></tr>`
        );
    }

    let nFiles = 0, totBytes = 0;
    for (const e of entries) {
        const full = sdJoin(sdPath, e.name);
        if (e.dir) {
            rows.push(
                `<tr class="row"><td class="name dir" onclick="sdNavTo('${esc(full)}')">` +
                `<span class="icon">📁</span>${esc(e.name)}</td>` +
                `<td class="size">—</td>` +
                `<td class="actions">` +
                `<button class="act" onclick="event.stopPropagation();sdRename('${esc(full)}','${esc(e.name)}')" title="Rename">✏️</button>` +
                `<button class="act del" onclick="event.stopPropagation();sdDelete('${esc(full)}','${esc(e.name)}',true)" title="Delete">🗑</button>` +
                `</td></tr>`
            );
        } else {
            nFiles++; totBytes += e.size || 0;
            const dl = "/api/sd/file?path=" + encodeURIComponent(full);
            const edit = isEditable(e.name)
                ? `<button class="act" onclick='openEditor("sd",${jsArg(e.name)},${jsArg(full)})' title="Edit">✎</button>`
                : "";
            rows.push(
                `<tr class="row">` +
                `<td class="name"><span class="icon">📄</span>${esc(e.name)}</td>` +
                `<td class="size">${fmtSize(e.size || 0)}</td>` +
                `<td class="actions">` +
                `<button class="act copy" onclick='copyToSpiffs(${jsArg(e.name)},${e.size || 0},${jsArg(full)})' title="Copy to SPIFFS ←">⬅</button>` +
                edit +
                `<a class="act" href="${dl}" title="Download">⬇</a>` +
                `<button class="act" onclick="sdRename('${esc(full)}','${esc(e.name)}')" title="Rename">✏️</button>` +
                `<button class="act del" onclick="sdDelete('${esc(full)}','${esc(e.name)}',false)" title="Delete">🗑</button>` +
                `</td></tr>`
            );
        }
    }

    if (rows.length === 0) sdShow("Empty folder.");
    else sdListEl.innerHTML = `<table class="files"><tbody>${rows.join("")}</tbody></table>`;
    sdMetaEl.textContent = `${nFiles} file${nFiles === 1 ? "" : "s"} · ${fmtSize(totBytes)}`;
}

async function sdRename(path, name) {
    const next = prompt("Rename to:", name);
    if (next === null) return;
    const t = next.trim();
    if (!t || t === name) return;
    if (t.includes("/") || t.includes("..")) { alert("Invalid name."); return; }
    try {
        const r = await fetch("/api/sd/rename?path=" + encodeURIComponent(path) +
                              "&to=" + encodeURIComponent(t), { method: "POST" });
        if (r.status === 503) { alert("No SD card."); return; }
        if (!r.ok) { alert("Rename failed (" + r.status + ")."); return; }
        sdRefresh();
    } catch (e) { alert("Connection error."); }
}

async function sdNewFolder() {
    const name = prompt("New folder name:");
    if (name === null) return;
    const t = name.trim();
    if (!t || t.includes("/") || t.includes("..")) { alert("Invalid folder name."); return; }
    try {
        const r = await fetch("/api/sd/mkdir?path=" + encodeURIComponent(sdJoin(sdPath, t)), { method: "POST" });
        if (r.status === 503) { alert("No SD card."); return; }
        if (!r.ok) { alert("Create failed (" + r.status + ")."); return; }
        sdRefresh();
    } catch (e) { alert("Connection error."); }
}

async function sdDelete(path, name, isDir) {
    const what = isDir ? "folder" : "file";
    if (!confirm(`Delete ${what} "${name}"?` + (isDir ? "\n(and its contents)" : ""))) return;
    try {
        const r = await fetch("/api/sd/file?path=" + encodeURIComponent(path), { method: "DELETE" });
        if (r.status === 503) { alert("No SD card."); return; }
        if (!r.ok) { alert("Delete failed (" + r.status + ")."); return; }
        sdRefresh();
    } catch (e) { alert("Connection error."); }
}

document.getElementById("sd_upload").addEventListener("change", async (ev) => {
    const files = Array.from(ev.target.files);
    ev.target.value = "";
    for (let i = 0; i < files.length; i++) await sdUploadOne(files[i], i + 1, files.length);
    sdBarEl.style.width = "0";
    sdRefresh();
});

function sdUploadOne(file, idx, count) {
    return new Promise((resolve) => {
        const dest = sdJoin(sdPath, file.name);
        const xhr = new XMLHttpRequest();
        xhr.open("POST", "/api/sd/file?path=" + encodeURIComponent(dest));
        xhr.upload.onprogress = (e) => {
            if (e.lengthComputable) sdBarEl.style.width = (100 * e.loaded / e.total) + "%";
            sdMetaEl.textContent = `Uploading ${idx}/${count}: ${file.name}`;
        };
        xhr.onload = () => {
            if (xhr.status === 503) alert("No SD card.");
            else if (xhr.status < 200 || xhr.status >= 300) alert(`Upload of ${file.name} failed (${xhr.status}).`);
            resolve();
        };
        xhr.onerror = () => { alert("Upload error: " + file.name); resolve(); };
        xhr.send(file);
    });
}

// ── transfer (per-file copy buttons) ────────────────────────────────────────
// SPIFFS → SD: read the (browser-decompressed) SPIFFS file, write it into the
// SD folder currently shown in the right pane.
async function copyToSd(name) {
    const dest = sdJoin(sdPath, name);

    // Warn before clobbering an existing SD file of the same name.
    if (await sdExists(name)) {
        if (!confirm(`"${name}" already exists in ${sdPath}. Overwrite?`)) return;
    }

    setLog(`Copying ${name} → SD…`);
    try {
        const r = await fetch("/" + encodePath(name), { cache: "no-store" });
        if (!r.ok) throw new Error("read HTTP " + r.status);
        const blob = await r.blob();
        const w = await fetch("/api/sd/file?path=" + encodeURIComponent(dest), {
            method: "POST", body: blob
        });
        if (w.status === 503) throw new Error("no SD card");
        if (!w.ok) throw new Error("write HTTP " + w.status);
        setLog(`✓ Copied ${name} to SD (${sdPath})`, "ok");
        sdRefresh();
    } catch (e) {
        setLog(`✗ ${name} → SD failed: ${e.message}`, "err");
    }
}

// SD → SPIFFS: read the SD file as text and PUT it to /api/files (the server
// re-gzips html/css/js). Text formats only — mirror the server's limits.
async function copyToSpiffs(name, size, srcPath) {
    const reason = spiffsRejectReason(name, size);
    if (reason) { setLog(`✗ Can't copy ${name} to SPIFFS: ${reason}`, "err"); return; }

    if (await spiffsExists(name)) {
        if (!confirm(`"${name}" already exists in SPIFFS. Overwrite (will replace the live UI file)?`)) return;
    }

    setLog(`Copying ${name} → SPIFFS…`);
    try {
        const r = await fetch("/api/sd/file?path=" + encodeURIComponent(srcPath));
        if (r.status === 503) throw new Error("no SD card");
        if (!r.ok) throw new Error("read HTTP " + r.status);
        const text = await r.text();
        const w = await fetch("/api/files/" + encodePath(name), {
            method: "PUT",
            headers: { "Content-Type": "text/plain; charset=utf-8" },
            body: text
        });
        if (!w.ok) {
            const err = await w.text().catch(() => "");
            throw new Error("write HTTP " + w.status + (err ? " — " + err : ""));
        }
        const info = await w.json().catch(() => null);
        setLog(`✓ Copied ${name} to SPIFFS` + (info ? ` (${info.gz ? "gz " : ""}${fmtSize(info.size)})` : ""), "ok");
        loadSpiffs();
    } catch (e) {
        setLog(`✗ ${name} → SPIFFS failed: ${e.message}`, "err");
    }
}

async function sdExists(name) {
    try {
        const r = await fetch("/api/sd/list?path=" + encodeURIComponent(sdPath));
        if (!r.ok) return false;
        const data = await r.json();
        return (data.entries || []).some(e => !e.dir && e.name === name);
    } catch (_) { return false; }
}

async function spiffsExists(name) {
    try {
        const r = await fetch("/api/files", { cache: "no-store" });
        if (!r.ok) return false;
        const list = await r.json();
        return list.some(f => f.name === name);
    } catch (_) { return false; }
}

// Backup every SPIFFS file into /backup/<timestamp>/ on the SD card. The SD POST
// handler auto-creates the parent dirs, so no explicit mkdir is needed. Files are
// stored decompressed (same as the per-file ⮕ copy) so the backup is readable and
// restorable via the ⬅ button. favicon.ico is excluded — /api/files doesn't list it.
const btnBackup = document.getElementById("btn_backup");

function backupStamp() {
    const d = new Date();
    const p = n => String(n).padStart(2, "0");
    return `${d.getFullYear()}-${p(d.getMonth() + 1)}-${p(d.getDate())}_` +
           `${p(d.getHours())}-${p(d.getMinutes())}-${p(d.getSeconds())}`;
}

async function backupAll() {
    let list;
    try {
        const r = await fetch("/api/files", { cache: "no-store" });
        if (!r.ok) throw new Error("HTTP " + r.status);
        list = await r.json();
    } catch (e) {
        setLog("✗ Backup failed: can't list SPIFFS (" + e.message + ")", "err");
        return;
    }
    if (!list.length) { setLog("Nothing to back up.", "warn"); return; }

    const dir = "/backup/" + backupStamp();
    btnBackup.disabled = true;
    let ok = 0, fail = 0;
    try {
        for (let i = 0; i < list.length; i++) {
            const name = list[i].name;
            setLog(`Backing up ${i + 1}/${list.length}: ${name}…`);
            try {
                const g = await fetch("/" + encodePath(name), { cache: "no-store" });
                if (!g.ok) throw new Error("read " + g.status);
                const blob = await g.blob();
                const w = await fetch("/api/sd/file?path=" + encodeURIComponent(dir + "/" + name), {
                    method: "POST", body: blob
                });
                if (w.status === 503) throw new Error("no SD card");
                if (!w.ok) throw new Error("write " + w.status);
                ok++;
            } catch (e) {
                if (/no SD card/.test(e.message)) {
                    setLog(`✗ Backup aborted: ${e.message}`, "err");
                    return;
                }
                fail++;
            }
        }
        setLog(`✓ Backed up ${ok} file${ok === 1 ? "" : "s"} to ${dir}` +
               (fail ? ` (${fail} failed)` : ""), fail ? "warn" : "ok");
        if (ok) sdNavTo(dir);
    } finally {
        btnBackup.disabled = false;
    }
}

// ── inline editor modal (both panes) ─────────────────────────────────────────
let edSource = null;   // 'spiffs' | 'sd'
let edName   = null;
let edPath   = null;   // SD only
const modalEl    = document.getElementById("modal");
const edSrcEl    = document.getElementById("ed_src");
const edNameEl   = document.getElementById("ed_name");
const edCodeEl   = document.getElementById("ed_code");
const edStatusEl = document.getElementById("ed_status");
const edJsonEl   = document.getElementById("ed_json");
let edJsonTimer  = null;

function edSetStatus(msg, kind) {
    edStatusEl.className = "status" + (kind ? " " + kind : "");
    edStatusEl.textContent = msg;
}

function edSetJson(kind, msg) {
    if (!kind) { edJsonEl.className = "json-status hidden"; edJsonEl.textContent = ""; return; }
    edJsonEl.className = "json-status " + kind;
    edJsonEl.textContent = msg;
}

// Map a JSON.parse error to a 1-based line/col so the user can find the typo.
function edJsonErrLoc(text, err) {
    const m = /position\s+(\d+)/i.exec((err && err.message) || "");
    if (!m) return null;
    const pos = Math.min(parseInt(m[1], 10), text.length);
    let line = 1, col = 1;
    for (let i = 0; i < pos; i++) {
        if (text.charCodeAt(i) === 10) { line++; col = 1; } else { col++; }
    }
    return { line, col };
}

function edValidateJson(text) {
    if (!text.trim()) { edSetJson("ok", "✓ empty"); return; }
    try {
        JSON.parse(text);
        edSetJson("ok", "✓ valid JSON");
    } catch (e) {
        const loc = edJsonErrLoc(text, e);
        edSetJson("err", "✗ " + (loc ? "line " + loc.line + ", col " + loc.col + ": " : "") + e.message);
    }
}

async function openEditor(source, name, path) {
    edSource = source;
    edName   = name;
    edPath   = path || null;
    edSrcEl.textContent  = source === "spiffs" ? "SPIFFS" : "SD";
    edNameEl.textContent = name;
    edCodeEl.value = "";
    edSetStatus("Loading…");
    edSetJson(null);
    modalEl.classList.add("open");
    try {
        const url = source === "spiffs"
            ? "/" + encodePath(name)
            : "/api/sd/file?path=" + encodeURIComponent(path);
        const r = await fetch(url, { cache: "no-store" });
        if (r.status === 503) throw new Error("no SD card");
        if (!r.ok) throw new Error("HTTP " + r.status);
        let text = await r.text();

        let note = "";
        if (isJson(name) && text.trim()) {
            try { text = JSON.stringify(JSON.parse(text), null, 2); note = " (pretty-printed)"; }
            catch (_) { /* leave raw so the user can fix the syntax */ }
        }
        edCodeEl.value = text;
        edCodeEl.scrollTop = 0;
        edSetStatus("Loaded " + name + note, "ok");
        if (isJson(name)) edValidateJson(text);
        edCodeEl.focus();
    } catch (e) {
        edSetStatus("Load failed: " + e.message, "err");
    }
}

function closeEditor() {
    modalEl.classList.remove("open");
    edSource = edName = edPath = null;
}

async function saveEditor() {
    if (!edSource) return;
    const text = edCodeEl.value;

    // SPIFFS PUT caps the body at 64 KB — fail early with a clear message.
    if (edSource === "spiffs" && new Blob([text]).size > SPIFFS_MAX) {
        edSetStatus("Too large (> " + fmtSize(SPIFFS_MAX) + ")", "err");
        return;
    }
    edSetStatus("Saving…");
    try {
        if (edSource === "spiffs") {
            const r = await fetch("/api/files/" + encodePath(edName), {
                method: "PUT",
                headers: { "Content-Type": "text/plain; charset=utf-8" },
                body: text
            });
            if (!r.ok) {
                const err = await r.text().catch(() => "");
                throw new Error("HTTP " + r.status + (err ? " — " + err : ""));
            }
            const info = await r.json().catch(() => null);
            edSetStatus("Saved" + (info ? " (" + (info.gz ? "gz " : "") + fmtSize(info.size) + ")" : ""), "ok");
            loadSpiffs();
        } else {
            const r = await fetch("/api/sd/file?path=" + encodeURIComponent(edPath), {
                method: "POST",
                headers: { "Content-Type": "text/plain; charset=utf-8" },
                body: text
            });
            if (r.status === 503) throw new Error("no SD card");
            if (!r.ok) throw new Error("HTTP " + r.status);
            edSetStatus("Saved", "ok");
            sdRefresh();
        }
    } catch (e) {
        edSetStatus("Save failed: " + e.message, "err");
    }
}

edCodeEl.addEventListener("input", () => {
    if (!isJson(edName)) return;
    clearTimeout(edJsonTimer);
    edJsonTimer = setTimeout(() => edValidateJson(edCodeEl.value), 200);
});

edCodeEl.addEventListener("keydown", (e) => {
    if (e.key === "Tab") {
        e.preventDefault();
        const s = edCodeEl.selectionStart;
        const v = edCodeEl.value;
        edCodeEl.value = v.substring(0, s) + "    " + v.substring(edCodeEl.selectionEnd);
        edCodeEl.selectionStart = edCodeEl.selectionEnd = s + 4;
    }
    if (e.key.toLowerCase() === "s" && (e.ctrlKey || e.metaKey)) {
        e.preventDefault();
        saveEditor();
    }
});

document.addEventListener("keydown", (e) => {
    if (e.key === "Escape" && modalEl.classList.contains("open")) closeEditor();
});
modalEl.addEventListener("click", (e) => { if (e.target === modalEl) closeEditor(); });

// ── init ────────────────────────────────────────────────────────────────────
loadSpiffs();
sdRefresh();
