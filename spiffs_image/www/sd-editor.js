// SD card file manager — talks to /api/sd/* on the device.
// State: the current directory path, always starting and (for dirs) ending with
// a single slash semantics handled by joinPath().

let currentPath = "/";

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
    return new Promise((resolve) => {
        const dest = joinPath(currentPath, file.name);
        const xhr = new XMLHttpRequest();
        xhr.open("POST", "/api/sd/file?path=" + encodeURIComponent(dest));
        xhr.upload.onprogress = (e) => {
            if (e.lengthComputable) {
                barEl.style.width = (100 * e.loaded / e.total) + "%";
            }
            metaEl.textContent = `Uploading ${idx}/${count}: ${file.name}`;
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

refresh();
