// Shared client-side preview for LVGL v9 RGB565 .bin images — the format
// scripts/img2lvgl.py (and the Android slideshow) produce and the firmware
// renders as photo-frame slides / wallpapers. The browser can't display these
// natively, so we parse the 12-byte header and paint the pixels onto a <canvas>.
//
// Used by manager.js (SD file manager) and settings.js (wallpaper picker).
// Exposes a single global: LvBin.openPreview(sdRelPath) / LvBin.decodeToCanvas(buf).
(function (global) {
    const MAGIC = 0x19;        // LV_IMAGE_HEADER_MAGIC
    const CF_RGB565 = 0x12;    // LV_COLOR_FORMAT_RGB565

    // Decode an LVGL .bin ArrayBuffer into a fresh <canvas>. Throws on a bad
    // header / wrong colour format / truncated data.
    function decodeToCanvas(buf) {
        const dv = new DataView(buf);
        if (dv.byteLength < 12) throw new Error("file too small");
        const magic = dv.getUint8(0), cf = dv.getUint8(1);
        if (magic !== MAGIC)
            throw new Error("not an LVGL image (magic 0x" + magic.toString(16) + ")");
        if (cf !== CF_RGB565)
            throw new Error("unsupported colour format 0x" + cf.toString(16) + " (need RGB565)");
        const w = dv.getUint16(4, true);
        const h = dv.getUint16(6, true);
        const stride = dv.getUint16(8, true) || w * 2;
        const need = 12 + stride * h;
        if (dv.byteLength < need)
            throw new Error("truncated (" + dv.byteLength + "/" + need + " bytes)");

        const canvas = document.createElement("canvas");
        canvas.width = w;
        canvas.height = h;
        const ctx = canvas.getContext("2d");
        const img = ctx.createImageData(w, h);
        const out = img.data;
        let o = 0;
        for (let y = 0; y < h; y++) {
            let p = 12 + y * stride;
            for (let x = 0; x < w; x++, p += 2) {
                const v = dv.getUint16(p, true);   // little-endian RGB565
                const r = (v >> 11) & 0x1f, g = (v >> 5) & 0x3f, b = v & 0x1f;
                out[o++] = (r * 527 + 23) >> 6;    // 5-bit → 8-bit
                out[o++] = (g * 259 + 33) >> 6;    // 6-bit → 8-bit
                out[o++] = (b * 527 + 23) >> 6;    // 5-bit → 8-bit
                out[o++] = 255;
            }
        }
        ctx.putImageData(img, 0, 0);
        return { canvas, w, h };
    }

    // Lazily-built modal overlay (one per page). Self-styled so it works on any
    // page that loads style.css for its CSS variables.
    let overlay, bodyEl, captionEl;
    function ensureModal() {
        if (overlay) return;
        overlay = document.createElement("div");
        overlay.style.cssText =
            "position:fixed;inset:0;background:rgba(0,0,0,.78);display:none;" +
            "align-items:center;justify-content:center;z-index:2000;padding:20px";
        const box = document.createElement("div");
        box.style.cssText =
            "background:var(--bg-panel,#1b1b1b);border:1px solid var(--border,#333);" +
            "border-radius:10px;padding:14px;max-width:96vw;max-height:96vh;" +
            "display:flex;flex-direction:column;gap:10px";
        const close = document.createElement("button");
        close.textContent = "✕ Close";
        close.className = "btn-secondary";
        close.onclick = hide;
        close.style.alignSelf = "flex-end";
        captionEl = document.createElement("div");
        captionEl.style.cssText = "font-family:monospace;font-size:12px;opacity:.85;text-align:center";
        bodyEl = document.createElement("div");
        bodyEl.style.cssText = "display:flex;align-items:center;justify-content:center;overflow:auto";
        box.append(close, bodyEl, captionEl);
        overlay.appendChild(box);
        overlay.addEventListener("click", (e) => { if (e.target === overlay) hide(); });
        document.addEventListener("keydown", (e) => {
            if (e.key === "Escape" && overlay.style.display !== "none") hide();
        });
        document.body.appendChild(overlay);
    }
    function hide() {
        if (!overlay) return;
        overlay.style.display = "none";
        bodyEl.innerHTML = "";
    }

    // Fetch + decode + show. `sdRelPath` is the mount-relative path the
    // /api/sd/file endpoint expects (e.g. "/wallpapers/10.bin").
    async function openPreview(sdRelPath) {
        ensureModal();
        bodyEl.innerHTML = "";
        captionEl.textContent = "Loading " + sdRelPath + " …";
        overlay.style.display = "flex";
        try {
            const r = await fetch("/api/sd/file?path=" + encodeURIComponent(sdRelPath), { cache: "no-store" });
            if (r.status === 503) throw new Error("no SD card");
            if (!r.ok) throw new Error("HTTP " + r.status);
            const { canvas, w, h } = decodeToCanvas(await r.arrayBuffer());
            canvas.style.cssText =
                "max-width:92vw;max-height:80vh;image-rendering:pixelated;" +
                "border:1px solid var(--border,#333);border-radius:6px;background:#000";
            bodyEl.appendChild(canvas);
            captionEl.textContent = sdRelPath.split("/").pop() + "  ·  " + w + "×" + h;
        } catch (e) {
            captionEl.textContent = "✗ Preview failed: " + e.message;
        }
    }

    global.LvBin = { decodeToCanvas, openPreview };
})(window);
