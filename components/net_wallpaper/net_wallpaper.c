#include "net_wallpaper.h"
#include "net_wallpaper_sched.h"
#include "app_state.h"
#include "radio_service.h"
#include "sdcard.h"
#include "esp_timer.h"
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <setjmp.h>
#include "jpeglib.h"    // vendored IJG libjpeg (components/libjpeg) — handles progressive

static const char *TAG = "NET_WP";

#define URL_MAX          512
#define APOD_JSON_MAX    (16 * 1024)     // APOD envelope (explanation text can be long)
#define JPEG_MAX         (1536 * 1024)   // download cap — APOD `url` images are ~100–500 KB
#define HTTP_TIMEOUT_MS  15000

// ── State ────────────────────────────────────────────────────────────────────
static volatile bool s_busy;
static const char   *s_status = "idle";  // "idle"/"busy"/"ok" or points at s_err
static char          s_err[96];

static char s_url[URL_MAX];              // request captured by net_wallpaper_fetch
static int  s_panel_w, s_panel_h;

// Finished fetch → LVGL task handoff. The fetch task publishes the decoded
// buffer under the spinlock; net_wallpaper_commit() (LVGL task) adopts it.
static portMUX_TYPE   s_mux = portMUX_INITIALIZER_UNLOCKED;
static uint16_t      *s_pending;
static int            s_pending_w, s_pending_h;
static bool           s_dismiss;   // drop the image at the next commit (user picked another bg)

static uint16_t      *s_active_buf;      // buffer behind s_img, owned here
static lv_image_dsc_t s_img;
static bool           s_have;

// Guards the LIFETIME of s_active_buf between the LVGL task (commit frees or
// replaces it) and the httpd task (save-to-SD snapshots it). Held only for
// pointer swaps and one ~434 KB memcpy — never across SD I/O.
static SemaphoreHandle_t s_buf_lock;

static SemaphoreHandle_t buf_lock(void)
{
    if (!s_buf_lock) {
        SemaphoreHandle_t m = xSemaphoreCreateMutex();
        taskENTER_CRITICAL(&s_mux);
        if (!s_buf_lock) { s_buf_lock = m; m = NULL; }
        taskEXIT_CRITICAL(&s_mux);
        if (m) vSemaphoreDelete(m);   // lost the creation race
    }
    return s_buf_lock;
}

static void (*s_done_cb)(bool ok);
static void (*s_start_cb)(void);

static void set_err(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s_err, sizeof(s_err), fmt, ap);
    va_end(ap);
    s_status = s_err;
    ESP_LOGW(TAG, "%s", s_err);
}

// ── Download ─────────────────────────────────────────────────────────────────
// GET `url` into a fresh PSRAM buffer (caller frees), following up to 3
// redirects manually — the open/fetch_headers flow doesn't auto-follow, and
// both picsum and the NASA image hosts answer with 30x. A body that fills the
// whole cap is treated as too large. Returns NULL with s_status set on error.
static uint8_t *download(const char *url, size_t cap, int *out_len)
{
    esp_http_client_config_t cfg = {
        .url               = url,
        .timeout_ms        = HTTP_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .user_agent        = "AtlasCube/1.0",
    };
    esp_http_client_handle_t cli = esp_http_client_init(&cfg);
    if (!cli) { set_err("http client init failed"); return NULL; }

    int status = 0;
    for (int hop = 0; hop < 4; hop++) {
        esp_err_t err = esp_http_client_open(cli, 0);
        if (err != ESP_OK) {
            set_err("connect failed (%s)", esp_err_to_name(err));
            esp_http_client_cleanup(cli);
            return NULL;
        }
        esp_http_client_fetch_headers(cli);
        status = esp_http_client_get_status_code(cli);
        if (status / 100 != 3) break;
        esp_http_client_set_redirection(cli);   // Location header → client URL
        esp_http_client_close(cli);
    }
    if (status / 100 != 2) {
        set_err("HTTP %d", status);
        esp_http_client_close(cli);
        esp_http_client_cleanup(cli);
        return NULL;
    }

    uint8_t *buf = heap_caps_malloc(cap, MALLOC_CAP_SPIRAM);
    if (!buf) {
        set_err("no PSRAM for %u B download", (unsigned)cap);
        esp_http_client_close(cli);
        esp_http_client_cleanup(cli);
        return NULL;
    }

    int total = 0;
    while ((size_t)total < cap) {
        int r = esp_http_client_read(cli, (char *)buf + total, cap - total);
        if (r < 0) { set_err("read failed at %d B", total); goto fail; }
        if (r == 0) break;
        total += r;
    }
    if ((size_t)total >= cap) { set_err("file larger than %u KB cap", (unsigned)(cap / 1024)); goto fail; }

    esp_http_client_close(cli);
    esp_http_client_cleanup(cli);
    *out_len = total;
    return buf;

fail:
    heap_caps_free(buf);
    esp_http_client_close(cli);
    esp_http_client_cleanup(cli);
    return NULL;
}

// ── NASA APOD ────────────────────────────────────────────────────────────────
// api.nasa.gov/planetary/apod answers with a JSON envelope, not an image. Pull
// the standard-resolution `url` out of it (`hdurl` can exceed both the download
// cap and the decoder's 1/8 scale limit) and guard against video days.
static bool apod_resolve(const char *api_url, char *out, size_t cap)
{
    int len = 0;
    uint8_t *body = download(api_url, APOD_JSON_MAX, &len);
    if (!body) return false;

    cJSON *root = cJSON_ParseWithLength((const char *)body, len);
    heap_caps_free(body);
    if (!root) { set_err("APOD: bad JSON"); return false; }

    bool ok = false;
    const cJSON *mt  = cJSON_GetObjectItem(root, "media_type");
    const cJSON *url = cJSON_GetObjectItem(root, "url");
    if (cJSON_IsString(mt) && strcmp(mt->valuestring, "image") != 0) {
        set_err("APOD: today is a %s, not an image", mt->valuestring);
    } else if (!cJSON_IsString(url) || !url->valuestring[0]) {
        set_err("APOD: no image url");
    } else if (strlen(url->valuestring) >= cap) {
        set_err("APOD: image url too long");
    } else {
        strcpy(out, url->valuestring);
        ok = true;
    }
    cJSON_Delete(root);
    return ok;
}

// ── Decode ───────────────────────────────────────────────────────────────────
// libjpeg reports errors by calling error_exit, which must not return —
// longjmp back into decode_to_panel instead. format_message fills a printable
// error string ("Unsupported JPEG data precision", …) for the status line.
struct decode_err_mgr {
    struct jpeg_error_mgr pub;
    jmp_buf jb;
};

static void decode_error_exit(j_common_ptr cinfo)
{
    struct decode_err_mgr *e = (struct decode_err_mgr *)cinfo->err;
    char msg[JMSG_LENGTH_MAX];
    (*cinfo->err->format_message)(cinfo, msg);
    set_err("jpeg: %s", msg);
    longjmp(e->jb, 1);
}

// Decode a JPEG (baseline or progressive — vendored IJG libjpeg) into a
// freshly allocated panel_w×panel_h RGB565 buffer in PSRAM (caller frees).
// The library's DCT-domain scaling (N/8, N=1..16) brings the image close to
// covering the panel; rows then stream through an RGB888 line buffer and are
// cropped/centered into the panel buffer on the fly — no full-size
// intermediate. Sources smaller than the panel get a black letterbox.
static uint16_t *decode_to_panel(uint8_t *jpg, int len, int pw, int ph)
{
    struct jpeg_decompress_struct cinfo;
    struct decode_err_mgr jerr;

    // Anything freed after a longjmp must be reachable there: keep the
    // pointers volatile so the setjmp return path sees their latest values.
    uint16_t *volatile panel = NULL;
    uint8_t  *volatile row   = NULL;

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = decode_error_exit;
    if (setjmp(jerr.jb)) {
        jpeg_destroy_decompress(&cinfo);
        if (row) heap_caps_free(row);
        if (panel) heap_caps_free(panel);
        return NULL;   // s_status was set in decode_error_exit
    }

    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, jpg, (unsigned long)len);
    jpeg_read_header(&cinfo, TRUE);
    ESP_LOGI(TAG, "JPEG %dx%d%s -> panel %dx%d", (int)cinfo.image_width,
             (int)cinfo.image_height, cinfo.progressive_mode ? " progressive" : "",
             pw, ph);

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
            set_err("progressive %ux%u too big: needs ~%u MB, ~%u MB free",
                    (unsigned)cinfo.image_width, (unsigned)cinfo.image_height,
                    (unsigned)((coef + 512 * 1024) / (1024 * 1024)),
                    (unsigned)(free_psram / (1024 * 1024)));
            longjmp(jerr.jb, 1);
        }
    }

    // Pick the smallest N/8 whose output still covers the panel ("cover", not
    // "fit"); N>8 upscales a source that is smaller than the panel, capped at
    // the library's 2x so a tiny image degrades to a letterbox, not to mush.
    int n = 1;
    while (n < 16 &&
           ((int)cinfo.image_width  * n / 8 < pw ||
            (int)cinfo.image_height * n / 8 < ph)) {
        n++;
    }
    cinfo.scale_num   = (unsigned)n;
    cinfo.scale_denom = 8;
    cinfo.out_color_space = JCS_RGB;

    jpeg_start_decompress(&cinfo);
    const int dw = (int)cinfo.output_width;
    const int dh = (int)cinfo.output_height;

    row = heap_caps_malloc((size_t)dw * 3, MALLOC_CAP_SPIRAM);
    if (!row) { set_err("no PSRAM for row buffer"); longjmp(jerr.jb, 1); }
    panel = heap_caps_calloc((size_t)pw * ph, 2, MALLOC_CAP_SPIRAM);   // zeroed → black letterbox
    if (!panel) { set_err("no PSRAM for wallpaper buffer"); longjmp(jerr.jb, 1); }

    // Centered crop window: source rows [sy, sy+ch) land on panel rows
    // [dy, dy+ch), columns likewise.
    const int cw = (dw < pw) ? dw : pw;
    const int ch = (dh < ph) ? dh : ph;
    const int sx = (dw - cw) / 2, sy = (dh - ch) / 2;
    const int dx = (pw - cw) / 2, dy = (ph - ch) / 2;

    while (cinfo.output_scanline < cinfo.output_height) {
        JSAMPROW rows[1] = { (JSAMPROW)row };
        jpeg_read_scanlines(&cinfo, rows, 1);
        const int y = (int)cinfo.output_scanline - 1;
        if (y < sy || y >= sy + ch) continue;

        const uint8_t *src = row + (size_t)sx * 3;
        uint16_t *dst = panel + (size_t)(dy + (y - sy)) * pw + dx;
        for (int x = 0; x < cw; x++, src += 3) {
            *dst++ = (uint16_t)(((src[0] >> 3) << 11) |
                                ((src[1] >> 2) << 5)  |
                                 (src[2] >> 3));
        }
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    heap_caps_free(row);
    return panel;
}

// ── Fetch task ───────────────────────────────────────────────────────────────
// Replace `{w}`/`{h}` in the URL with the panel size, so a single saved URL
// (e.g. picsum.photos/{w}/{h}) fits every display variant.
static void expand_url(const char *in, char *out, size_t cap, int w, int h)
{
    size_t n = 0;
    for (const char *p = in; *p && n < cap - 1; ) {
        if (strncmp(p, "{w}", 3) == 0 || strncmp(p, "{h}", 3) == 0) {
            n += snprintf(out + n, cap - n, "%d", (p[1] == 'w') ? w : h);
            if (n > cap - 1) n = cap - 1;   // snprintf returns the untruncated length
            p += 3;
        } else {
            out[n++] = *p++;
        }
    }
    out[n] = '\0';
}

static bool do_fetch(const char *url)
{
    char img_url[URL_MAX];
    if (strstr(url, "api.nasa.gov/planetary/apod")) {
        if (!apod_resolve(url, img_url, sizeof(img_url))) return false;
        ESP_LOGI(TAG, "APOD image: %s", img_url);
        url = img_url;
    }

    int len = 0;
    uint8_t *jpg = download(url, JPEG_MAX, &len);
    if (!jpg) return false;
    ESP_LOGI(TAG, "downloaded %d B", len);

    uint16_t *panel = decode_to_panel(jpg, len, s_panel_w, s_panel_h);
    heap_caps_free(jpg);
    if (!panel) return false;

    taskENTER_CRITICAL(&s_mux);
    uint16_t *stale = s_pending;             // unclaimed previous fetch, if any
    s_pending   = panel;
    s_pending_w = s_panel_w;
    s_pending_h = s_panel_h;
    taskEXIT_CRITICAL(&s_mux);
    if (stale) heap_caps_free(stale);
    return true;
}

static void fetch_task(void *arg)
{
    (void)arg;
    char url[URL_MAX];
    expand_url(s_url, url, sizeof(url), s_panel_w, s_panel_h);
    ESP_LOGI(TAG, "fetching %s", url);
    if (s_start_cb) s_start_cb();            // UI pill: explain the coming silence

    // Stop a playing radio stream for the duration: its (possibly TLS) socket
    // plus ours is exactly the two-HTTPS-sessions pattern that used to starve
    // internal RAM. Restore afterwards, same as the voice-notification path.
    app_state_t *st = app_state_get();
    bool was_radio = (st->radio_state == RADIO_STATE_PLAYING ||
                      st->radio_state == RADIO_STATE_BUFFERING);
    int  prev_idx  = st->curr_index;
    if (was_radio) radio_stop();

    bool ok = do_fetch(url);

    if (was_radio) radio_play_index(prev_idx);

    if (ok) s_status = "ok";                 // errors were set where they occurred
    s_busy = false;
    if (s_done_cb) s_done_cb(ok);
    net_wallpaper_sched_fetch_done(ok);      // scheduler retry/re-arm hook
    vTaskDelete(NULL);
}

// ── Public API ───────────────────────────────────────────────────────────────
bool net_wallpaper_fetch(const char *url, int panel_w, int panel_h)
{
    if (!url || !url[0] || panel_w <= 0 || panel_h <= 0) return false;
    if (s_busy) return false;
    s_busy = true;

    strncpy(s_url, url, sizeof(s_url) - 1);
    s_url[sizeof(s_url) - 1] = '\0';
    s_panel_w = panel_w;
    s_panel_h = panel_h;
    s_status  = "busy";

    // Internal-RAM stack: the TLS handshake runs on this task via
    // esp_http_client, and mbedtls needs a few KB of headroom.
    if (xTaskCreate(fetch_task, "net_wp", 10240, NULL, 5, NULL) != pdPASS) {
        set_err("task create failed");
        s_busy = false;
        return false;
    }
    return true;
}

const char *net_wallpaper_status(void)
{
    return s_status;
}

const lv_image_dsc_t *net_wallpaper_image(void)
{
    return s_have ? &s_img : NULL;
}

void net_wallpaper_commit(void)
{
    taskENTER_CRITICAL(&s_mux);
    uint16_t *p = s_pending;
    int w = s_pending_w, h = s_pending_h;
    bool dismiss = s_dismiss;
    s_pending = NULL;
    s_dismiss = false;
    taskEXIT_CRITICAL(&s_mux);

    // An explicit background choice outranks everything, including a fetch
    // that happened to finish in the meantime — drop both image and pending.
    if (dismiss) {
        if (p) heap_caps_free(p);
        xSemaphoreTake(buf_lock(), portMAX_DELAY);
        if (s_have) {
            lv_image_cache_drop(&s_img);
            s_have = false;
        }
        if (s_active_buf) {
            heap_caps_free(s_active_buf);
            s_active_buf = NULL;
        }
        xSemaphoreGive(s_buf_lock);
        ESP_LOGI(TAG, "wallpaper dismissed");
        return;
    }
    if (!p) return;

    xSemaphoreTake(buf_lock(), portMAX_DELAY);
    if (s_have) lv_image_cache_drop(&s_img);
    if (s_active_buf) heap_caps_free(s_active_buf);
    s_active_buf = p;

    s_img.header.magic  = LV_IMAGE_HEADER_MAGIC;
    s_img.header.cf     = LV_COLOR_FORMAT_RGB565;
    s_img.header.w      = w;
    s_img.header.h      = h;
    s_img.header.stride = w * 2;
    s_img.data_size     = (uint32_t)w * h * 2;
    s_img.data          = (const uint8_t *)p;
    s_have = true;
    xSemaphoreGive(s_buf_lock);
    ESP_LOGI(TAG, "wallpaper committed (%dx%d)", w, h);
}

// ── Save to SD ───────────────────────────────────────────────────────────────
// LVGL v9 .bin header, byte-compatible with scripts/img2lvgl.py output and the
// SD-wallpaper/photo-screensaver loaders (see ui_background.c).
typedef struct __attribute__((packed)) {
    uint8_t  magic;      // 0x19
    uint8_t  cf;         // 0x12 = RGB565
    uint16_t flags;
    uint16_t w;
    uint16_t h;
    uint16_t stride;
    uint16_t reserved;
} sd_bin_header_t;

#define SAVE_DIR "/sdcard/wallpapers/saved"

bool net_wallpaper_save_to_sd(char *out_path, size_t out_cap, const char **err)
{
    *err = NULL;
    // Also closes the buffer-swap race: a new image can only be committed at
    // the tail of a fetch, and no fetch is running past this check.
    if (s_busy) { *err = "fetch in progress"; return false; }
    if (sdcard_init() != ESP_OK || !sdcard_is_mounted()) { *err = "no SD card"; return false; }

    // Snapshot under the buffer lock (one fast memcpy), then write the file
    // from the copy with no lock held — the SD write takes ~a second.
    xSemaphoreTake(buf_lock(), portMAX_DELAY);
    if (!s_have) {
        xSemaphoreGive(s_buf_lock);
        *err = "no wallpaper fetched";
        return false;
    }
    const int w = s_img.header.w, h = s_img.header.h;
    const size_t bytes = (size_t)w * h * 2;
    uint16_t *copy = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
    if (copy) memcpy(copy, s_active_buf, bytes);
    xSemaphoreGive(s_buf_lock);
    if (!copy) { *err = "no PSRAM"; return false; }

    mkdir("/sdcard/wallpapers", 0775);   // EEXIST is fine for both levels
    mkdir(SAVE_DIR, 0775);

    time_t now = time(NULL);
    struct tm lt;
    localtime_r(&now, &lt);
    if (lt.tm_year + 1900 >= 2020) {
        snprintf(out_path, out_cap, SAVE_DIR "/net_%04d%02d%02d_%02d%02d%02d.bin",
                 lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday,
                 lt.tm_hour, lt.tm_min, lt.tm_sec);
    } else {   // clock not NTP-synced yet — fall back to a boot-unique stamp
        snprintf(out_path, out_cap, SAVE_DIR "/net_%08lx.bin",
                 (unsigned long)(esp_timer_get_time() / 1000));
    }

    FILE *fp = fopen(out_path, "wb");
    if (!fp) {
        heap_caps_free(copy);
        *err = "create failed";
        return false;
    }
    sd_bin_header_t hdr = {
        .magic  = 0x19,
        .cf     = 0x12,
        .w      = (uint16_t)w,
        .h      = (uint16_t)h,
        .stride = (uint16_t)(w * 2),
    };
    bool ok = fwrite(&hdr, sizeof(hdr), 1, fp) == 1 &&
              fwrite(copy, 2, (size_t)w * h, fp) == (size_t)w * h;
    fclose(fp);
    heap_caps_free(copy);
    if (!ok) {
        unlink(out_path);   // don't leave a truncated .bin behind
        *err = "write failed";
        return false;
    }
    ESP_LOGI(TAG, "wallpaper saved to %s", out_path);
    return true;
}

uint8_t *net_wallpaper_bin_snapshot(size_t *out_len)
{
    xSemaphoreTake(buf_lock(), portMAX_DELAY);
    if (!s_have) {
        xSemaphoreGive(s_buf_lock);
        return NULL;
    }
    const int w = s_img.header.w, h = s_img.header.h;
    const size_t bytes = (size_t)w * h * 2;
    uint8_t *bin = heap_caps_malloc(sizeof(sd_bin_header_t) + bytes, MALLOC_CAP_SPIRAM);
    if (bin) {
        const sd_bin_header_t hdr = {
            .magic  = 0x19,
            .cf     = 0x12,
            .w      = (uint16_t)w,
            .h      = (uint16_t)h,
            .stride = (uint16_t)(w * 2),
        };
        memcpy(bin, &hdr, sizeof(hdr));
        memcpy(bin + sizeof(hdr), s_active_buf, bytes);
    }
    xSemaphoreGive(s_buf_lock);
    if (!bin) return NULL;
    *out_len = sizeof(sd_bin_header_t) + bytes;
    return bin;
}

void net_wallpaper_set_done_cb(void (*cb)(bool ok))
{
    s_done_cb = cb;
}

void net_wallpaper_set_start_cb(void (*cb)(void))
{
    s_start_cb = cb;
}

void net_wallpaper_dismiss(void)
{
    taskENTER_CRITICAL(&s_mux);
    s_dismiss = true;
    taskEXIT_CRITICAL(&s_mux);
}
