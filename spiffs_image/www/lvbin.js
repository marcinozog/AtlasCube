// Shared client-side preview for LVGL v9 .bin images — the format
// scripts/img2lvgl.py (and the Android slideshow) produce and the firmware
// renders as photo-frame slides / wallpapers. The browser can't display these
// natively, so we parse the 12-byte header and paint the pixels onto a <canvas>.
//
// Two colour formats: RGB565 for anything opaque (wallpapers, slides) and
// RGB565A8 for artwork that needs transparency (slider knobs) — the colour
// plane followed by the alpha plane, exactly the layout the firmware's internet
// asset slots decode a PNG into, so a knob looks the same whichever route it
// took to the device.
//
// Used by manager.js (SD file manager) and settings.js (wallpaper picker).
// Exposes one global with preview/decoder helpers plus encodeImage(blob, w, h),
// which converts a browser-supported image to the same crop-to-cover format
// used by scripts/img2lvgl.py and AtlasCube Remote.
(function (global) {
    const MAGIC = 0x19;        // LV_IMAGE_HEADER_MAGIC
    const CF_RGB565 = 0x12;    // LV_COLOR_FORMAT_RGB565
    const CF_RGB565A8 = 0x14;  // LV_COLOR_FORMAT_RGB565A8 — colour plane, then alpha plane

    // Decode an LVGL .bin ArrayBuffer into a fresh <canvas>. Throws on a bad
    // header / wrong colour format / truncated data.
    function decodeToCanvas(buf) {
        const dv = new DataView(buf);
        if (dv.byteLength < 12) throw new Error("file too small");
        const magic = dv.getUint8(0), cf = dv.getUint8(1);
        if (magic !== MAGIC)
            throw new Error("not an LVGL image (magic 0x" + magic.toString(16) + ")");
        if (cf !== CF_RGB565 && cf !== CF_RGB565A8)
            throw new Error("unsupported colour format 0x" + cf.toString(16) +
                            " (need RGB565 or RGB565A8)");
        const w = dv.getUint16(4, true);
        const h = dv.getUint16(6, true);
        const stride = dv.getUint16(8, true) || w * 2;
        // The header's stride describes the colour plane; LVGL takes the alpha
        // stride as half of it.
        const alpha = cf === CF_RGB565A8;
        const alphaBase = 12 + stride * h;
        const alphaStride = stride >> 1;
        const need = alphaBase + (alpha ? alphaStride * h : 0);
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
            let a = alphaBase + y * alphaStride;
            for (let x = 0; x < w; x++, p += 2) {
                const v = dv.getUint16(p, true);   // little-endian RGB565
                const r = (v >> 11) & 0x1f, g = (v >> 5) & 0x3f, b = v & 0x1f;
                out[o++] = (r * 527 + 23) >> 6;    // 5-bit → 8-bit
                out[o++] = (g * 259 + 33) >> 6;    // 6-bit → 8-bit
                out[o++] = (b * 527 + 23) >> 6;    // 5-bit → 8-bit
                out[o++] = alpha ? dv.getUint8(a++) : 255;
            }
        }
        ctx.putImageData(img, 0, 0);
        return { canvas, w, h, alpha };
    }

    async function decodeImage(blob) {
        if (typeof createImageBitmap === "function") {
            try { return await createImageBitmap(blob, { imageOrientation: "from-image" }); }
            catch (_) { /* fall back to an HTMLImageElement */ }
        }
        return await new Promise((resolve, reject) => {
            const url = URL.createObjectURL(blob);
            const img = new Image();
            img.onload = () => { URL.revokeObjectURL(url); resolve(img); };
            img.onerror = () => {
                URL.revokeObjectURL(url);
                reject(new Error("browser cannot decode this image"));
            };
            img.src = url;
        });
    }

    // A canvas stores its pixels premultiplied, so it hands back colour 0 for
    // anything fully transparent. Bilinear scaling on the device resamples the
    // colour and the alpha planes independently, and would therefore drag those
    // black pixels into the sprite's edge as a dark fringe. Bleed the nearest
    // opaque colour one step outwards; the alpha plane is left alone, so nothing
    // becomes visible that was not visible before.
    function bleedTransparentEdges(rgba, w, h) {
        const src = rgba.slice();      // read the original, write the target
        for (let y = 0; y < h; y++) {
            for (let x = 0; x < w; x++) {
                const i = (y * w + x) * 4;
                if (src[i + 3] !== 0) continue;
                let found = -1;
                for (let dy = -1; dy <= 1 && found < 0; dy++) {
                    for (let dx = -1; dx <= 1; dx++) {
                        const nx = x + dx, ny = y + dy;
                        if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
                        const n = (ny * w + nx) * 4;
                        if (src[n + 3] !== 0) { found = n; break; }
                    }
                }
                if (found < 0) continue;
                rgba[i]     = src[found];
                rgba[i + 1] = src[found + 1];
                rgba[i + 2] = src[found + 2];
            }
        }
    }

    // Center-crop to cover the target panel, then serialize the pixels into an
    // LVGL v9 binary image (12-byte LE header + row-major LE pixels).
    //
    // `keepAlpha` asks for transparency to survive: a source that actually has
    // non-opaque pixels comes out as RGB565A8 (colour plane, then alpha plane),
    // while a JPEG or a fully opaque PNG still comes out as plain RGB565 — no
    // point spending 50% more card space and draw time on an all-255 plane.
    // Wallpapers never pass it, so they are unaffected.
    async function encodeImage(blob, w, h, keepAlpha) {
        w = Number(w); h = Number(h);
        if (!Number.isInteger(w) || !Number.isInteger(h) || w < 1 || h < 1 ||
            w > 32767 || h > 32767) throw new Error("invalid panel dimensions");

        const src = await decodeImage(blob);
        const sw = src.width || src.naturalWidth;
        const sh = src.height || src.naturalHeight;
        if (!sw || !sh) throw new Error("image has invalid dimensions");

        const canvas = document.createElement("canvas");
        canvas.width = w; canvas.height = h;
        const ctx = canvas.getContext("2d", { alpha: !!keepAlpha });
        if (!ctx) throw new Error("canvas is unavailable");
        ctx.imageSmoothingEnabled = true;
        ctx.imageSmoothingQuality = "high";
        const scale = Math.max(w / sw, h / sh);
        const dw = Math.max(w, Math.round(sw * scale));
        const dh = Math.max(h, Math.round(sh * scale));
        ctx.drawImage(src, Math.round((w - dw) / 2), Math.round((h - dh) / 2), dw, dh);
        if (typeof src.close === "function") src.close();

        const rgba = ctx.getImageData(0, 0, w, h).data;
        let alpha = false;
        if (keepAlpha) {
            for (let i = 3; i < rgba.length; i += 4)
                if (rgba[i] !== 255) { alpha = true; break; }
        }
        if (alpha) bleedTransparentEdges(rgba, w, h);

        const px = w * h;
        const out = new ArrayBuffer(12 + px * (alpha ? 3 : 2));
        const dv = new DataView(out);
        dv.setUint8(0, MAGIC);
        dv.setUint8(1, alpha ? CF_RGB565A8 : CF_RGB565);
        dv.setUint16(2, 0, true);
        dv.setUint16(4, w, true);
        dv.setUint16(6, h, true);
        dv.setUint16(8, w * 2, true);   // colour stride; the alpha plane uses half
        dv.setUint16(10, 0, true);
        let p = 12;
        let a = 12 + px * 2;
        for (let i = 0; i < rgba.length; i += 4) {
            const v = ((rgba[i] & 0xf8) << 8) |
                      ((rgba[i + 1] & 0xfc) << 3) |
                      (rgba[i + 2] >> 3);
            dv.setUint16(p, v, true);
            p += 2;
            if (alpha) dv.setUint8(a++, rgba[i + 3]);
        }
        return new Blob([out], { type: "application/octet-stream" });
    }

    // Sanitize an original filename into an ASCII stem safe for FATFS paths
    // and the JSON config files that reference them.
    function fileStem(name) {
        const raw = String(name || "").replace(/\.[^.]*$/, "");
        // NFKD handles most Polish diacritics, but Ł/ł needs an explicit mapping.
        const latin = raw.replace(/ł/g, "l").replace(/Ł/g, "L");
        const ascii = latin.normalize ? latin.normalize("NFKD").replace(/[\u0300-\u036f]/g, "") : latin;
        return ascii.replace(/[^a-zA-Z0-9_-]+/g, "-").replace(/^-+|-+$/g, "").slice(0, 80) ||
               ("wallpaper-" + Date.now());
    }

    // Convert a browser image to a panel-sized .bin and store it on the SD
    // card under `dir` (mount-relative, e.g. "/wallpapers"). Returns the
    // mount-relative path of the uploaded file. `onStatus`, when given, gets
    // progress messages for the caller's status line. `keepAlpha` is passed
    // straight to encodeImage() — set it for artwork, not for wallpapers.
    async function uploadImage(file, dir, w, h, onStatus, saveAs, keepAlpha) {
        const note = typeof onStatus === "function" ? onStatus : () => {};
        const stem = fileStem(saveAs || file.name);
        note("Converting to " + stem + ".bin…");
        const bin = await encodeImage(file, w, h, keepAlpha);
        const base = String(dir || "").replace(/\/+$/, "");
        const relPath = base + "/" + stem + ".bin";
        note("Uploading " + w + "×" + h + " as " + stem + ".bin…");
        const r = await fetch("/api/sd/file?path=" + encodeURIComponent(relPath), {
            method: "POST", body: bin
        });
        if (r.status === 503) throw new Error("no SD card");
        if (!r.ok) throw new Error("SD upload HTTP " + r.status);
        return relPath;
    }

    // Lazily-built modal overlay (one per page). Self-styled so it works on any
    // page that loads style.css for its CSS variables.
    let overlay, bodyEl, captionEl;
    function ensureModal() {
        if (overlay) return;
        overlay = document.createElement("div");
        overlay.style.cssText =
            "position:fixed;inset:0;background:rgba(0,0,0,.78);display:none;" +
            // Sits above the SD picker modal (z-index 1900) so previews launched
            // from a file row show on top rather than hidden behind it.
            "align-items:center;justify-content:center;z-index:2200;padding:20px";
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
            const { canvas, w, h, alpha } = decodeToCanvas(await r.arrayBuffer());
            // Transparent artwork gets a checkerboard rather than solid black —
            // on black you cannot tell a transparent knob from a black one.
            const ground = alpha
                ? "background-color:#222;background-size:16px 16px;" +
                  "background-position:0 0,0 8px,8px -8px,-8px 0;background-image:" +
                  "linear-gradient(45deg,#444 25%,transparent 25%)," +
                  "linear-gradient(-45deg,#444 25%,transparent 25%)," +
                  "linear-gradient(45deg,transparent 75%,#444 75%)," +
                  "linear-gradient(-45deg,transparent 75%,#444 75%)"
                : "background:#000";
            canvas.style.cssText =
                "max-width:92vw;max-height:80vh;image-rendering:pixelated;" +
                "border:1px solid var(--border,#333);border-radius:6px;" + ground;
            bodyEl.appendChild(canvas);
            captionEl.textContent = sdRelPath.split("/").pop() + "  ·  " + w + "×" + h +
                                    "  ·  " + (alpha ? "RGB565A8" : "RGB565");
        } catch (e) {
            captionEl.textContent = "✗ Preview failed: " + e.message;
        }
    }

    global.LvBin = { decodeToCanvas, encodeImage, uploadImage, openPreview, fileStem };
})(window);
