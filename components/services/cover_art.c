#include "cover_art.h"
#include "sdcard.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include <setjmp.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "jpeglib.h"      // vendored IJG libjpeg (components/libjpeg) — after stdio, it needs FILE

static const char *TAG = "COVER_ART";

// Stored square size. Matches what the web uploader writes, so both routes to
// a cover.bin produce the same file and the widget rescales either to whatever
// the layout asks for.
#define COVER_PX      240

// Fixed, short list of accepted spellings — the names album downloads actually
// use. Anything else is set from the web UI, which writes cover.bin directly.
static const char *k_sources[] = { "cover.jpg", "cover.jpeg", "folder.jpg", "front.jpg" };

#define COVER_DIR_LEN 192           // matches app_state's sd_dir
#define COVER_PATH_LEN (COVER_DIR_LEN + 16)

static char s_dir[COVER_DIR_LEN];   // folder being converted (owned by the task while busy)
static char s_tried[COVER_DIR_LEN]; // last folder with no usable source — don't ask the card again
static volatile bool s_busy;
static void (*s_done_cb)(void);

void cover_art_set_done_cb(void (*cb)(void)) { s_done_cb = cb; }

// ── Decode ───────────────────────────────────────────────────────────────────
// libjpeg reports errors by calling error_exit, which must not return — longjmp
// back into decode_square instead.
struct decode_err_mgr {
    struct jpeg_error_mgr pub;
    jmp_buf jb;
};

static void decode_error_exit(j_common_ptr cinfo)
{
    struct decode_err_mgr *e = (struct decode_err_mgr *)cinfo->err;
    char msg[JMSG_LENGTH_MAX];
    (*cinfo->err->format_message)(cinfo, msg);
    ESP_LOGW(TAG, "jpeg: %s", msg);
    longjmp(e->jb, 1);
}

// Decode `path` into a freshly allocated COVER_PX square of RGB565 in PSRAM
// (caller frees). The library's DCT-domain scaling (N/8) brings the image close
// to covering the square; rows then stream through an RGB888 line buffer and
// are cropped/centered on the fly, so nothing full-size is ever held. Reads
// straight from the card — a cover JPEG never lands in RAM whole.
static uint16_t *decode_square(const char *path)
{
    struct jpeg_decompress_struct cinfo;
    struct decode_err_mgr jerr;

    // Anything freed after a longjmp must be reachable there: keep the pointers
    // volatile so the setjmp return path sees their latest values.
    uint16_t *volatile out = NULL;
    uint8_t  *volatile row = NULL;
    FILE     *volatile fp  = fopen(path, "rb");
    if (!fp) return NULL;

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = decode_error_exit;
    if (setjmp(jerr.jb)) {
        jpeg_destroy_decompress(&cinfo);
        fclose(fp);
        if (row) heap_caps_free(row);
        if (out) heap_caps_free(out);
        return NULL;
    }

    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, fp);
    jpeg_read_header(&cinfo, TRUE);

    // Album art is never this big; anything that is, is not worth the PSRAM a
    // progressive decode of it would ask for.
    if (cinfo.image_width > 4000 || cinfo.image_height > 4000) {
        ESP_LOGW(TAG, "%s: %ux%u is too large", path,
                 (unsigned)cinfo.image_width, (unsigned)cinfo.image_height);
        longjmp(jerr.jb, 1);
    }

    // Smallest N/8 whose output still covers the square ("cover", not "fit");
    // N>8 upscales a source smaller than the square, capped at the library's 2x.
    int n = 1;
    while (n < 16 &&
           ((int)cinfo.image_width  * n / 8 < COVER_PX ||
            (int)cinfo.image_height * n / 8 < COVER_PX)) {
        n++;
    }
    cinfo.scale_num       = (unsigned)n;
    cinfo.scale_denom     = 8;
    cinfo.out_color_space = JCS_RGB;

    jpeg_start_decompress(&cinfo);
    const int dw = (int)cinfo.output_width;
    const int dh = (int)cinfo.output_height;

    row = heap_caps_malloc((size_t)dw * 3, MALLOC_CAP_SPIRAM);
    if (!row) longjmp(jerr.jb, 1);
    out = heap_caps_calloc((size_t)COVER_PX * COVER_PX, 2, MALLOC_CAP_SPIRAM);  // zeroed → black letterbox
    if (!out) longjmp(jerr.jb, 1);

    // Centered crop window: source rows [sy, sy+ch) land on rows [dy, dy+ch),
    // columns likewise. A source smaller than the square keeps a black border.
    const int cw = (dw < COVER_PX) ? dw : COVER_PX;
    const int ch = (dh < COVER_PX) ? dh : COVER_PX;
    const int sx = (dw - cw) / 2, sy = (dh - ch) / 2;
    const int dx = (COVER_PX - cw) / 2, dy = (COVER_PX - ch) / 2;

    while (cinfo.output_scanline < cinfo.output_height) {
        JSAMPROW rows[1] = { (JSAMPROW)row };
        jpeg_read_scanlines(&cinfo, rows, 1);
        const int y = (int)cinfo.output_scanline - 1;
        if (y < sy || y >= sy + ch) continue;

        const uint8_t *src = row + (size_t)sx * 3;
        uint16_t *dst = out + (size_t)(dy + (y - sy)) * COVER_PX + dx;
        for (int x = 0; x < cw; x++, src += 3) {
            *dst++ = (uint16_t)(((src[0] >> 3) << 11) |
                                ((src[1] >> 2) << 5)  |
                                 (src[2] >> 3));
        }
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(fp);
    heap_caps_free(row);
    return out;
}

// ── Store ────────────────────────────────────────────────────────────────────
// LVGL v9 image header — the same 12 bytes the web uploader and
// scripts/img2lvgl.py write, so every route produces one readable format.
typedef struct __attribute__((packed)) {
    uint8_t  magic;      // 0x19
    uint8_t  cf;         // 0x12 = RGB565
    uint16_t flags;
    uint16_t w;
    uint16_t h;
    uint16_t stride;
    uint16_t reserved;
} cover_bin_header_t;

static bool write_bin(const char *path, const uint16_t *px)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) return false;

    const cover_bin_header_t hdr = {
        .magic  = 0x19,
        .cf     = 0x12,
        .w      = COVER_PX,
        .h      = COVER_PX,
        .stride = COVER_PX * 2,
    };
    const size_t px_count = (size_t)COVER_PX * COVER_PX;
    bool ok = fwrite(&hdr, sizeof(hdr), 1, fp) == 1 &&
              fwrite(px, 2, px_count, fp) == px_count;
    fclose(fp);
    if (!ok) unlink(path);   // don't leave a truncated .bin behind
    return ok;
}

// ── Worker ───────────────────────────────────────────────────────────────────
// First accepted JPEG in `dir`, or false when the folder has none.
static bool find_source(const char *dir, char *out, size_t cap)
{
    for (size_t i = 0; i < sizeof(k_sources) / sizeof(k_sources[0]); i++) {
        struct stat st;
        snprintf(out, cap, "%s/%s", dir, k_sources[i]);
        if (stat(out, &st) == 0 && !S_ISDIR(st.st_mode)) return true;
    }
    return false;
}

static void convert_task(void *arg)
{
    (void)arg;
    char src[COVER_PATH_LEN];
    char dst[COVER_PATH_LEN];
    bool ok = false;

    if (find_source(s_dir, src, sizeof(src))) {
        ESP_LOGI(TAG, "converting %s", src);
        uint16_t *px = decode_square(src);
        if (px) {
            snprintf(dst, sizeof(dst), "%s/cover.bin", s_dir);
            ok = write_bin(dst, px);
            heap_caps_free(px);
            if (ok) ESP_LOGI(TAG, "wrote %s", dst);
            else    ESP_LOGW(TAG, "cannot write %s", dst);
        }
    }

    // Remember a folder that yielded nothing, so browsing back and forth over
    // an album without artwork doesn't hit the card again and again.
    if (!ok) snprintf(s_tried, sizeof(s_tried), "%s", s_dir);

    s_busy = false;
    if (ok && s_done_cb) s_done_cb();
    vTaskDelete(NULL);
}

void cover_art_request(const char *dir)
{
    if (!dir || !dir[0] || s_busy) return;
    if (strcmp(dir, s_tried) == 0) return;
    if (sdcard_init() != ESP_OK) return;

    snprintf(s_dir, sizeof(s_dir), "%s", dir);
    s_busy = true;
    // Short-lived: one album's worth of work, then the task is gone. libjpeg's
    // own allocations go to PSRAM (jmem_esp), so the stack only carries the
    // decoder's call frames.
    if (xTaskCreate(convert_task, "cover_art", 6144, NULL, 4, NULL) != pdPASS) {
        ESP_LOGW(TAG, "cannot start the converter");
        s_busy = false;
    }
}
