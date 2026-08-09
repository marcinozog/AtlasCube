/*
 * JPEG -> RGB565 for this project's two image consumers: the internet
 * wallpaper (a downloaded photo scaled onto the panel) and the SD player's
 * album art (a cover.jpg on the card scaled into a square). Both want the same
 * thing — cover-crop into a fixed-size RGB565 buffer in PSRAM — and differ only
 * in where the bytes come from, so the decode lives here rather than twice in
 * the callers.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "jpeglib.h"
#include "jpeg_rgb565.h"

static const char *TAG = "JPEG";

// libjpeg reports errors by calling error_exit, which must not return — longjmp
// back into decode() instead, carrying a printable message for the caller.
struct rgb565_err_mgr {
    struct jpeg_error_mgr pub;
    jmp_buf jb;
    char   *err;
    size_t  cap;
};

static void set_err(struct rgb565_err_mgr *e, const char *fmt, ...)
{
    if (!e->err || !e->cap) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(e->err, e->cap, fmt, ap);
    va_end(ap);
}

static void decode_error_exit(j_common_ptr cinfo)
{
    struct rgb565_err_mgr *e = (struct rgb565_err_mgr *)cinfo->err;
    char msg[JMSG_LENGTH_MAX];
    (*cinfo->err->format_message)(cinfo, msg);
    set_err(e, "jpeg: %s", msg);
    longjmp(e->jb, 1);
}

// Exactly one source is given: `fp` (streamed from the file, nothing buffered
// whole) or `buf`/`len`.
static uint16_t *decode(FILE *fp, const uint8_t *buf, size_t len,
                        int dw_out, int dh_out, char *err, size_t err_cap)
{
    struct jpeg_decompress_struct cinfo;
    struct rgb565_err_mgr jerr;

    if (dw_out <= 0 || dh_out <= 0) return NULL;
    if (err && err_cap) err[0] = '\0';

    // Anything freed after a longjmp must be reachable there: keep the pointers
    // volatile so the setjmp return path sees their latest values.
    uint16_t *volatile out = NULL;
    uint8_t  *volatile row = NULL;

    jerr.err = err;
    jerr.cap = err_cap;
    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = decode_error_exit;
    if (setjmp(jerr.jb)) {
        jpeg_destroy_decompress(&cinfo);
        if (row) heap_caps_free(row);
        if (out) heap_caps_free(out);
        return NULL;   // the message was set on the way here
    }

    jpeg_create_decompress(&cinfo);
    if (fp) jpeg_stdio_src(&cinfo, fp);
    else    jpeg_mem_src(&cinfo, (unsigned char *)buf, (unsigned long)len);
    jpeg_read_header(&cinfo, TRUE);
    ESP_LOGI(TAG, "%ux%u%s -> %dx%d", (unsigned)cinfo.image_width,
             (unsigned)cinfo.image_height,
             cinfo.progressive_mode ? " progressive" : "", dw_out, dh_out);

    // A progressive (or any multi-scan) JPEG buffers the whole image's DCT
    // coefficients across scans — the one hard size limit on this device
    // (~1.5 MP with 8 MB PSRAM; baseline decodes line-by-line at any size).
    // Estimate the need up front and fail with a readable message instead of
    // jmemmgr's cryptic backing-store error.
    if (jpeg_has_multiple_scans(&cinfo)) {
        size_t coef = 0;
        for (int c = 0; c < cinfo.num_components; c++) {
            const jpeg_component_info *comp = &cinfo.comp_info[c];
            size_t cols = ((size_t)cinfo.image_width  * comp->h_samp_factor
                           / cinfo.max_h_samp_factor + 7) / 8 + 1;
            size_t rows = ((size_t)cinfo.image_height * comp->v_samp_factor
                           / cinfo.max_v_samp_factor + 7) / 8 + 1;
            coef += cols * rows * (DCTSIZE2 * sizeof(JCOEF));
        }
        const size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        if (coef + 1024 * 1024 > free_psram) {   // same 1 MB headroom as jmem_esp
            set_err(&jerr, "progressive %ux%u too big: needs ~%u MB, ~%u MB free",
                    (unsigned)cinfo.image_width, (unsigned)cinfo.image_height,
                    (unsigned)((coef + 512 * 1024) / (1024 * 1024)),
                    (unsigned)(free_psram / (1024 * 1024)));
            longjmp(jerr.jb, 1);
        }
    }

    // Pick the smallest N/8 whose output still covers the target ("cover", not
    // "fit"); N>8 upscales a source smaller than the target, capped at the
    // library's 2x so a tiny image degrades to a letterbox, not to mush.
    int n = 1;
    while (n < 16 &&
           ((int)cinfo.image_width  * n / 8 < dw_out ||
            (int)cinfo.image_height * n / 8 < dh_out)) {
        n++;
    }
    cinfo.scale_num       = (unsigned)n;
    cinfo.scale_denom     = 8;
    cinfo.out_color_space = JCS_RGB;

    jpeg_start_decompress(&cinfo);
    const int dw = (int)cinfo.output_width;
    const int dh = (int)cinfo.output_height;

    row = heap_caps_malloc((size_t)dw * 3, MALLOC_CAP_SPIRAM);
    if (!row) { set_err(&jerr, "no PSRAM for row buffer"); longjmp(jerr.jb, 1); }
    out = heap_caps_calloc((size_t)dw_out * dh_out, 2, MALLOC_CAP_SPIRAM);   // zeroed → black letterbox
    if (!out) { set_err(&jerr, "no PSRAM for %dx%d image", dw_out, dh_out); longjmp(jerr.jb, 1); }

    // Centered crop window: source rows [sy, sy+ch) land on output rows
    // [dy, dy+ch), columns likewise.
    const int cw = (dw < dw_out) ? dw : dw_out;
    const int ch = (dh < dh_out) ? dh : dh_out;
    const int sx = (dw - cw) / 2, sy = (dh - ch) / 2;
    const int dx = (dw_out - cw) / 2, dy = (dh_out - ch) / 2;

    while (cinfo.output_scanline < cinfo.output_height) {
        JSAMPROW rows[1] = { (JSAMPROW)row };
        jpeg_read_scanlines(&cinfo, rows, 1);
        const int y = (int)cinfo.output_scanline - 1;
        if (y < sy || y >= sy + ch) continue;

        const uint8_t *src = row + (size_t)sx * 3;
        uint16_t *dst = out + (size_t)(dy + (y - sy)) * dw_out + dx;
        for (int x = 0; x < cw; x++, src += 3) {
            *dst++ = (uint16_t)(((src[0] >> 3) << 11) |
                                ((src[1] >> 2) << 5)  |
                                 (src[2] >> 3));
        }
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    heap_caps_free(row);
    return out;
}

uint16_t *jpeg_file_to_rgb565(const char *path, int dst_w, int dst_h,
                              char *err, size_t err_cap)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        if (err && err_cap) snprintf(err, err_cap, "cannot open %s", path);
        return NULL;
    }
    uint16_t *out = decode(fp, NULL, 0, dst_w, dst_h, err, err_cap);
    fclose(fp);
    return out;
}

uint16_t *jpeg_mem_to_rgb565(const uint8_t *buf, size_t len, int dst_w, int dst_h,
                             char *err, size_t err_cap)
{
    if (!buf || !len) return NULL;
    return decode(NULL, buf, len, dst_w, dst_h, err, err_cap);
}
