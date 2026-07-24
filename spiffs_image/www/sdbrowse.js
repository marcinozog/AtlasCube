// sdbrowse.js — shared navigable SD-card folder browser.
//
// One place to render a file explorer over /api/sd/list: the user can step
// through folders (a ".." row plus subdirectories) and act on files. Every
// read-only picker in the UI renders through here — the wallpaper picker, the
// knob-image picker, the splash-logo picker and the Assets tab — so new
// explorer features (preview, delete, move, mkdir, …) only have to be added
// once and they show up everywhere.
//
//   SdBrowse.open(container, options) -> controller
//
// options:
//   start        initial mount-relative directory (default '/').
//   fallback     directory to retry once if `start` fails to load — fresh
//                cards may lack /wallpapers etc. (default: no fallback).
//   filter       (entry) => boolean: which *files* to show. `entry` is a
//                {name, dir, …} record from /api/sd/list. Folders always show.
//   filterExt    convenience: only files whose name ends with this suffix,
//                case-insensitive (e.g. '.bin'). Ignored when `filter` is set.
//   fileIcon     icon prefix for file rows (default '📄 ').
//   fileLabel    (fullPath, entry) => string: full custom row label, overrides
//                fileIcon (e.g. to flag the active wallpaper with a ✓).
//   onFile       (fullPath, entry) => void: click on a file row.
//   fileActions  (fullPath, entry) => [{label, title?, className?, onClick(ev)}]
//                extra buttons rendered on the right of a file row.
//   onDirChange  (dir) => void: after each successful navigation.
//   emptyText    message shown when a folder has no visible entries.
//   rowFontSize  CSS font-size for rows (default '12px').
//   maxHeight    CSS max-height for the scroll area (default '200px').
//
// Returns a controller: { refresh(), go(dir), current() }.
(function () {
    'use strict';

    // Directories first, then names naturally (numeric-aware, case-insensitive).
    const byDirThenName = (a, b) =>
        (!!b.dir - !!a.dir) ||
        a.name.localeCompare(b.name, undefined, { numeric: true, sensitivity: 'base' });

    const parentOf = dir => dir.replace(/\/[^/]+\/?$/, '') || '/';
    const joinDir  = (dir, name) => (dir.endsWith('/') ? dir : dir + '/') + name;

    function open(container, options) {
        const opts      = options || {};
        const fileIcon  = opts.fileIcon || '\u{1F4C4} ';   // 📄
        const rowFont   = opts.rowFontSize || '12px';
        const maxHeight = opts.maxHeight || '200px';
        const accept = opts.filter
            ? opts.filter
            : (opts.filterExt
                ? e => e.name.toLowerCase().endsWith(opts.filterExt.toLowerCase())
                : () => true);

        let current = opts.start || '/';
        let usedFallback = false;

        async function load(dir, allowFallback) {
            container.textContent = 'Loading ' + dir + ' …';
            let data;
            try {
                const r = await fetch('/api/sd/list?path=' + encodeURIComponent(dir), { cache: 'no-store' });
                if (!r.ok) throw new Error('HTTP ' + r.status);
                data = await r.json();
            } catch (err) {
                if (allowFallback && opts.fallback && dir !== opts.fallback && !usedFallback) {
                    usedFallback = true;
                    return load(opts.fallback, false);
                }
                container.innerHTML = '';
                const msg = document.createElement('div');
                msg.className = 'field-hint';
                msg.textContent = 'SD folder unavailable: ' + err.message;
                container.appendChild(msg);
                return;
            }
            current = data.path || dir || '/';
            if (opts.onDirChange) opts.onDirChange(current);
            render(data);
        }

        function render(data) {
            container.innerHTML = '';

            const heading = document.createElement('div');
            heading.textContent = current;
            heading.style.cssText =
                'font-family:monospace;font-size:11px;opacity:.75;margin-bottom:4px';

            const list = document.createElement('div');
            list.style.cssText =
                'max-height:' + maxHeight + ';overflow:auto;border:1px solid var(--border);' +
                'border-radius:var(--radius-sm,6px);background:var(--bg-card)';

            const addNav = (label, targetDir) => {
                const row = document.createElement('div');
                row.textContent = label;
                row.style.cssText = 'padding:6px 10px;cursor:pointer;font-size:' + rowFont;
                row.onmouseenter = () => { row.style.background = 'var(--bg-input)'; };
                row.onmouseleave = () => { row.style.background = ''; };
                row.addEventListener('click', () => load(targetDir, false));
                list.appendChild(row);
            };

            if (current !== '/') addNav('\u{1F4C1} ..', parentOf(current));   // 📁 ..

            const entries = (data.entries || []).slice().sort(byDirThenName);
            let files = 0, dirs = 0;
            for (const entry of entries) {
                const full = joinDir(current, entry.name);
                if (entry.dir) { dirs++; addNav('\u{1F4C1} ' + entry.name, full); continue; }
                if (!accept(entry)) continue;
                files++;
                list.appendChild(fileRow(full, entry));
            }

            if (!files && !dirs) {
                const empty = document.createElement('div');
                empty.style.cssText = 'padding:8px 10px;font-size:11px;color:var(--text-dim)';
                empty.textContent = opts.emptyText || 'This folder is empty.';
                list.appendChild(empty);
            }

            container.append(heading, list);
        }

        function fileRow(full, entry) {
            const row = document.createElement('div');
            const actions = opts.fileActions ? opts.fileActions(full, entry) : null;
            row.style.cssText =
                'padding:6px 10px;font-size:' + rowFont + ';display:flex;align-items:center;gap:8px' +
                (opts.onFile ? ';cursor:pointer' : '');
            row.onmouseenter = () => { row.style.background = 'var(--bg-input)'; };
            row.onmouseleave = () => { row.style.background = ''; };

            const name = document.createElement('span');
            name.style.flex = '1';
            name.textContent = opts.fileLabel ? opts.fileLabel(full, entry) : fileIcon + entry.name;
            if (opts.onFile) name.addEventListener('click', () => opts.onFile(full, entry));
            row.appendChild(name);

            if (actions && actions.length) {
                const wrap = document.createElement('span');
                wrap.style.cssText = 'display:flex;gap:6px';
                for (const a of actions) {
                    const b = document.createElement('button');
                    b.type = 'button';
                    b.className = a.className || 'btn-secondary';
                    b.textContent = a.label;
                    if (a.title) b.title = a.title;
                    b.onclick = ev => { ev.stopPropagation(); a.onClick(ev); };
                    wrap.appendChild(b);
                }
                row.appendChild(wrap);
            }
            return row;
        }

        load(current, true);

        return {
            refresh: () => load(current, false),
            go:      dir => load(dir || '/', false),
            current: () => current,
        };
    }

    window.SdBrowse = { open };
})();
