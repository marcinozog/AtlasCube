// SD card file manager — talks to /api/sd/* on the device.
// State: the current directory path, always starting and (for dirs) ending with
// a single slash semantics handled by joinPath().

// '?path=/music/...' lets the SD player deep-link into the folder it is browsing.
let currentPath = new URLSearchParams(location.search).get("path") || "/";
if (!currentPath.startsWith("/") || currentPath.includes("..")) currentPath = "/";

// Above this the copy round trip is slow enough (and the blob big enough) to be
// worth a confirmation rather than a silent stall.
const COPY_WARN_BYTES = 32 * 1024 * 1024;

const listingEl = document.getElementById("listing");
const crumbsEl  = document.getElementById("crumbs");
const metaEl    = document.getElementById("files_meta");
const barEl     = document.getElementById("upload_bar");

function joinPath(dir, name) {
    if (dir.endsWith("/")) return dir + name;
    return dir + "/" + name;
}

function parentPath(dir) {
    const p = dir.replace(/\/+$/, "");
    const i = p.lastIndexOf("/");
    return i <= 0 ? "/" : p.slice(0, i);
}

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

function showStatus(msg, isErr) {
    listingEl.innerHTML =
        `<div class="files-status${isErr ? " err" : ""}">${esc(msg)}</div>`;
}

function renderCrumbs() {
    const parts = currentPath.split("/").filter(Boolean);
    let html = `<a onclick="navTo('/')">SD</a>`;
    let acc = "";
    for (const part of parts) {
        acc += "/" + part;
        html += `<span class="sep">/</span><a onclick="navTo('${esc(acc)}')">${esc(part)}</a>`;
    }
    crumbsEl.innerHTML = html;
}

function navTo(path) {
    currentPath = path || "/";
    refresh();
}

async function refresh() {
    renderCrumbs();
    showStatus("Loading…");
    try {
        const r = await fetch("/api/sd/list?path=" + encodeURIComponent(currentPath));
        if (r.status === 503) { showStatus("No SD card inserted.", true); metaEl.textContent = "no card"; return; }
        if (!r.ok) { showStatus("Error: " + r.status, true); return; }
        const data = await r.json();
        renderList(data.entries || []);
    } catch (e) {
        showStatus("Connection error.", true);
    }
}

function renderList(entries) {
    entries.sort((a, b) => {
        if (a.dir !== b.dir) return a.dir ? -1 : 1;        // folders first
        return a.name.localeCompare(b.name, undefined, { numeric: true });
    });

    const rows = [];
    if (currentPath !== "/" && currentPath !== "") {
        rows.push(
            `<tr><td class="name dir" onclick="navTo('${esc(parentPath(currentPath))}')">` +
            `<span class="icon">📁</span>..</td><td class="size"></td><td class="actions"></td></tr>`
        );
    }

    let nFiles = 0, totBytes = 0;
    for (const e of entries) {
        const full = joinPath(currentPath, e.name);
        if (e.dir) {
            rows.push(
                `<tr><td class="name dir" onclick="navTo('${esc(full)}')">` +
                `<span class="icon">📁</span>${esc(e.name)}</td>` +
                `<td class="size">—</td>` +
                `<td class="actions">` +
                `<button class="act" onclick="event.stopPropagation();moveEntry('${esc(full)}','${esc(e.name)}',true)" title="Move">📂</button>` +
                `<button class="act" onclick="event.stopPropagation();renameEntry('${esc(full)}','${esc(e.name)}')" title="Rename">✏️</button>` +
                `<button class="act del" onclick="event.stopPropagation();delEntry('${esc(full)}','${esc(e.name)}',true)" title="Delete">🗑</button>` +
                `</td></tr>`
            );
        } else {
            nFiles++; totBytes += e.size || 0;
            const dl = "/api/sd/file?path=" + encodeURIComponent(full);
            rows.push(
                `<tr><td class="name"><span class="icon">📄</span>${esc(e.name)}</td>` +
                `<td class="size">${fmtSize(e.size || 0)}</td>` +
                `<td class="actions">` +
                `<a class="act" href="${dl}" title="Download">⬇</a>` +
                `<button class="act" onclick="copyEntry('${esc(full)}','${esc(e.name)}',${e.size || 0})" title="Copy">📋</button>` +
                `<button class="act" onclick="moveEntry('${esc(full)}','${esc(e.name)}',false)" title="Move">📂</button>` +
                `<button class="act" onclick="renameEntry('${esc(full)}','${esc(e.name)}')" title="Rename">✏️</button>` +
                `<button class="act del" onclick="delEntry('${esc(full)}','${esc(e.name)}',false)" title="Delete">🗑</button>` +
                `</td></tr>`
            );
        }
    }

    if (rows.length === 0) { showStatus("Empty folder."); }
    else {
        listingEl.innerHTML =
            `<table class="files"><thead><tr><th>Name</th><th style="text-align:right">Size</th><th></th></tr></thead>` +
            `<tbody>${rows.join("")}</tbody></table>`;
    }
    metaEl.textContent = `${nFiles} file${nFiles === 1 ? "" : "s"} · ${fmtSize(totBytes)}`;
}

async function renameEntry(path, name) {
    const next = prompt("Rename to:", name);
    if (next === null) return;
    const trimmed = next.trim();
    if (!trimmed || trimmed === name) return;
    if (trimmed.includes("/") || trimmed.includes("..")) { alert("Invalid name."); return; }
    try {
        const r = await fetch("/api/sd/rename?path=" + encodeURIComponent(path) +
                              "&to=" + encodeURIComponent(trimmed), { method: "POST" });
        if (r.status === 503) { alert("No SD card."); return; }
        if (!r.ok) { alert("Rename failed (" + r.status + ")."); return; }
        refresh();
    } catch (e) { alert("Connection error."); }
}

// Move a file/folder to another directory. The destination folder is typed in
// a prompt; missing levels are created via /api/sd/mkdir (one level at a time),
// then /api/sd/rename with a full '/'-prefixed target performs the move.
async function moveEntry(path, name, isDir) {
    const input = prompt("Move to folder:", parentPath(path));
    if (input === null) return;
    let dir = input.trim();
    if (!dir.startsWith("/")) dir = "/" + dir;
    dir = dir.replace(/\/+$/, "") || "/";
    if (dir.includes("..")) { alert("Invalid path."); return; }
    if (dir === parentPath(path)) return;                       // same folder — nothing to do
    if (isDir && (dir === path || dir.startsWith(path + "/"))) {
        alert("Cannot move a folder into itself."); return;
    }
    try {
        let acc = "";
        for (const part of dir.split("/").filter(Boolean)) {
            acc += "/" + part;
            const m = await fetch("/api/sd/mkdir?path=" + encodeURIComponent(acc), { method: "POST" });
            if (m.status === 503) { alert("No SD card."); return; }
            if (!m.ok) { alert(`Cannot create ${acc} (${m.status}).`); return; }
        }
        const r = await fetch("/api/sd/rename?path=" + encodeURIComponent(path) +
                              "&to=" + encodeURIComponent(joinPath(dir, name)), { method: "POST" });
        if (r.status === 503) { alert("No SD card."); return; }
        if (!r.ok) { alert(`Move failed (${r.status}). Target may already exist.`); return; }
        refresh();
    } catch (e) { alert("Connection error."); }
}

// Copy a file elsewhere on the card. The device exposes no server-side copy, so
// the bytes make a round trip through the browser — GET the source, POST it back
// under the new path — the same transfer manager.html uses between SPIFFS and SD.
// Files only: a recursive folder copy would mean one round trip per file.
async function copyEntry(path, name, size) {
    const input = prompt("Copy to folder:", parentPath(path));
    if (input === null) return;
    let dir = input.trim();
    if (!dir.startsWith("/")) dir = "/" + dir;
    dir = dir.replace(/\/+$/, "") || "/";
    if (dir.includes("..")) { alert("Invalid path."); return; }

    // Landing in the source folder needs a different name to not be a no-op.
    let target = name;
    if (dir === parentPath(path)) {
        const next = prompt("Copy as:", copyName(name));
        if (next === null) return;
        target = next.trim();
        if (!target || target.includes("/") || target.includes("..")) { alert("Invalid name."); return; }
        if (target === name) return;
    }

    if (size > COPY_WARN_BYTES &&
        !confirm(`${fmtSize(size)} has to travel through the browser and back. Continue?`)) return;

    try {
        if (await entryExists(dir, target) &&
            !confirm(`"${target}" already exists in ${dir}. Overwrite?`)) return;

        let acc = "";
        for (const part of dir.split("/").filter(Boolean)) {
            acc += "/" + part;
            const m = await fetch("/api/sd/mkdir?path=" + encodeURIComponent(acc), { method: "POST" });
            if (m.status === 503) { alert("No SD card."); return; }
            if (!m.ok) { alert(`Cannot create ${acc} (${m.status}).`); return; }
        }

        metaEl.textContent = `Reading ${name}…`;
        const r = await fetch("/api/sd/file?path=" + encodeURIComponent(path));
        if (r.status === 503) { alert("No SD card."); return; }
        if (!r.ok) { alert(`Cannot read ${name} (${r.status}).`); return; }
        const blob = await r.blob();

        await putBlob(blob, joinPath(dir, target), `Copying ${name} → ${dir}`);
    } catch (e) {
        alert("Connection error.");
    }
    barEl.style.width = "0";
    refresh();
}

function copyName(name) {
    const i = name.lastIndexOf(".");
    return i > 0 ? name.slice(0, i) + " (copy)" + name.slice(i) : name + " (copy)";
}

async function entryExists(dir, name) {
    const r = await fetch("/api/sd/list?path=" + encodeURIComponent(dir));
    if (!r.ok) return false;                       // missing folder — nothing to clobber
    const data = await r.json();
    return (data.entries || []).some(e => e.name === name);
}

async function newFolder() {
    const name = prompt("New folder name:");
    if (name === null) return;
    const trimmed = name.trim();
    if (!trimmed || trimmed.includes("/") || trimmed.includes("..")) { alert("Invalid folder name."); return; }
    try {
        const r = await fetch("/api/sd/mkdir?path=" + encodeURIComponent(joinPath(currentPath, trimmed)), { method: "POST" });
        if (r.status === 503) { alert("No SD card."); return; }
        if (!r.ok) { alert("Create failed (" + r.status + ")."); return; }
        refresh();
    } catch (e) { alert("Connection error."); }
}

async function delEntry(path, name, isDir) {
    const what = isDir ? "folder" : "file";
    if (!confirm(`Delete ${what} "${name}"?` + (isDir ? "\n(must be empty)" : ""))) return;
    try {
        const r = await fetch("/api/sd/file?path=" + encodeURIComponent(path), { method: "DELETE" });
        if (r.status === 503) { alert("No SD card."); return; }
        if (!r.ok) { alert("Delete failed (" + r.status + ")."); return; }
        refresh();
    } catch (e) { alert("Connection error."); }
}

// ── Format the card ─────────────────────────────────────────────────────────
// The one irreversible action on this page, so it goes through a modal that
// spells out what disappears and stays locked until the word is typed. The
// device wants the same intent restated as a confirm token on the request.
const fmtModal  = document.getElementById("fmt_modal");
const fmtToken  = document.getElementById("fmt_token");
const fmtGo     = document.getElementById("fmt_go");
const fmtCancel = document.getElementById("fmt_cancel");
const fmtStatus = document.getElementById("fmt_status");
let fmtBusy = false;

function openFormat() {
    fmtToken.value = "";
    fmtGo.disabled = true;
    setFmtStatus("");
    fmtModal.classList.remove("hidden");
    fmtToken.focus();
}

function closeFormat() {
    if (fmtBusy) return;                       // never leave a format unattended
    fmtModal.classList.add("hidden");
}

function setFmtStatus(msg, isErr) {
    fmtStatus.textContent = msg;
    fmtStatus.classList.toggle("err", !!isErr);
}

fmtToken.addEventListener("input", () => {
    fmtGo.disabled = fmtToken.value.trim().toUpperCase() !== "FORMAT";
});

fmtToken.addEventListener("keydown", (ev) => {
    if (ev.key === "Enter" && !fmtGo.disabled) runFormat();
});

document.addEventListener("keydown", (ev) => {
    if (ev.key === "Escape" && !fmtModal.classList.contains("hidden")) closeFormat();
});

fmtModal.addEventListener("click", (ev) => {
    if (ev.target === fmtModal) closeFormat();  // click outside the card = cancel
});

async function runFormat() {
    if (fmtBusy) return;
    fmtBusy = true;
    fmtGo.disabled = fmtCancel.disabled = fmtToken.disabled = true;
    setFmtStatus("Formatting… do not power off");
    metaEl.textContent = "formatting…";
    try {
        // No timeout on purpose: the device answers only once f_mkfs is done.
        const r = await fetch("/api/sd/format?confirm=erase-everything", { method: "POST" });
        if (r.status === 503) { setFmtStatus("No SD card.", true); }
        else if (!r.ok)       { setFmtStatus(`Format failed (${r.status}).`, true); }
        else {
            fmtModal.classList.add("hidden");
            currentPath = "/";                  // the old tree no longer exists
        }
    } catch (e) {
        setFmtStatus("Connection lost — check the device.", true);
    }
    fmtBusy = false;
    fmtGo.disabled = fmtCancel.disabled = fmtToken.disabled = false;
    fmtToken.value = "";
    fmtGo.disabled = true;
    refresh();
}

// Upload the selected files one by one into the current folder, with a progress
// bar (XHR gives us upload progress events that fetch() does not).
document.getElementById("upload_input").addEventListener("change", async (ev) => {
    const files = Array.from(ev.target.files);
    ev.target.value = "";          // allow re-selecting the same file later
    for (let i = 0; i < files.length; i++) {
        await uploadOne(files[i], i + 1, files.length);
    }
    barEl.style.width = "0";
    refresh();
});

function uploadOne(file, idx, count) {
    return putBlob(file, joinPath(currentPath, file.name),
                   `Uploading ${idx}/${count}: ${file.name}`);
}

// Streams a blob into `dest`, driving the progress bar; `label` is what the meta
// line shows while it runs. Shared by uploads and copies.
function putBlob(blob, dest, label) {
    return new Promise((resolve) => {
        const xhr = new XMLHttpRequest();
        xhr.open("POST", "/api/sd/file?path=" + encodeURIComponent(dest));
        xhr.upload.onprogress = (e) => {
            if (e.lengthComputable) {
                barEl.style.width = (100 * e.loaded / e.total) + "%";
            }
            metaEl.textContent = label;
        };
        xhr.onload = () => {
            if (xhr.status === 503) alert("No SD card.");
            else if (xhr.status < 200 || xhr.status >= 300) alert(`${label} — failed (${xhr.status}).`);
            resolve();
        };
        xhr.onerror = () => { alert("Transfer error: " + label); resolve(); };
        xhr.send(blob);
    });
}

refresh();
