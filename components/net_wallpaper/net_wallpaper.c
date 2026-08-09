#include "net_wallpaper.h"
#include "net_wallpaper_sched.h"
#include "net_asset.h"
#include "net_fetch.h"   // shared HTTP getter + the status line assets share
#include "settings.h"   // WALLPAPER_SLOTS + the per-slot URLs the batch fetch walks
#include "app_state.h"
#include "radio_service.h"
#include "sdcard.h"
#include "esp_timer.h"
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdio.h>
#include "jpeg_rgb565.h"   // shared decode-to-RGB565 (components/libjpeg, progressive-capable)

static const char *TAG = "NET_WP";

#define URL_MAX          512
#define APOD_JSON_MAX    (16 * 1024)     // APOD envelope (explanation text can be long)
#define JPEG_MAX         (1536 * 1024)   // download cap — APOD `url` images are ~100–500 KB

// ── State ────────────────────────────────────────────────────────────────────
static volatile bool s_busy;

static char s_url[URL_MAX];              // single-slot request captured by fetch()
static int  s_req_slot = -1;             // slot to fetch, or -1 for "every configured one"
static int  s_panel_w, s_panel_h;
static volatile int s_prog_done, s_prog_total;   // batch progress for the UI pill

// One independent wallpaper per slot. A slot only ever costs PSRAM once
// something has actually been fetched into it, so the ten-slot ceiling is free
// for anyone using one.
typedef struct {
    uint16_t      *buf;        // buffer behind img, owned here
    lv_image_dsc_t img;
    bool           have;
    uint16_t      *pending;    // decoded, waiting for the LVGL task to adopt it
    int            pend_w, pend_h;
    bool           dismiss;    // drop this slot at the next commit
} wp_slot_t;

// Finished fetch → LVGL task handoff. The fetch task publishes the decoded
// buffer under the spinlock; net_wallpaper_commit() (LVGL task) adopts it.
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static wp_slot_t    s_slot[NET_WP_SLOTS];

// Guards the LIFETIME of the committed buffers between the LVGL task (commit
// frees or replaces them) and the httpd task (save-to-SD snapshots one). Held
// only for pointer swaps and one ~434 KB memcpy — never across SD I/O. One lock
// for every slot: the critical sections are microseconds and never nested.
static SemaphoreHandle_t s_buf_lock;

static inline bool slot_valid(int slot) { return slot >= 0 && slot < NET_WP_SLOTS; }

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

// ── NASA APOD ────────────────────────────────────────────────────────────────
// api.nasa.gov/planetary/apod answers with a JSON envelope, not an image. Pull
// the standard-resolution `url` out of it (`hdurl` can exceed both the download
// cap and the decoder's 1/8 scale limit) and guard against video days.
static bool apod_resolve(const char *api_url, char *out, size_t cap)
{
    int len = 0;
    uint8_t *body = net_fetch_download(api_url, APOD_JSON_MAX, &len);
    if (!body) return false;

    cJSON *root = cJSON_ParseWithLength((const char *)body, len);
    heap_caps_free(body);
    if (!root) { net_fetch_set_err("APOD: bad JSON"); return false; }

    bool ok = false;
    const cJSON *mt  = cJSON_GetObjectItem(root, "media_type");
    const cJSON *url = cJSON_GetObjectItem(root, "url");
    if (cJSON_IsString(mt) && strcmp(mt->valuestring, "image") != 0) {
        net_fetch_set_err("APOD: today is a %s, not an image", mt->valuestring);
    } else if (!cJSON_IsString(url) || !url->valuestring[0]) {
        net_fetch_set_err("APOD: no image url");
    } else if (strlen(url->valuestring) >= cap) {
        net_fetch_set_err("APOD: image url too long");
    } else {
        strcpy(out, url->valuestring);
        ok = true;
    }
    cJSON_Delete(root);
    return ok;
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

static bool do_fetch(const char *url, int slot)
{
    char img_url[URL_MAX];
    if (strstr(url, "api.nasa.gov/planetary/apod")) {
        if (!apod_resolve(url, img_url, sizeof(img_url))) return false;
        ESP_LOGI(TAG, "APOD image: %s", img_url);
        url = img_url;
    }

    int len = 0;
    uint8_t *jpg = net_fetch_download(url, JPEG_MAX, &len);
    if (!jpg) return false;
    ESP_LOGI(TAG, "slot %d: downloaded %d B", slot, len);

    char err[96] = "";
    uint16_t *panel = jpeg_mem_to_rgb565(jpg, (size_t)len, s_panel_w, s_panel_h,
                                         err, sizeof(err));
    heap_caps_free(jpg);
    if (!panel) {
        net_fetch_set_err("%s", err[0] ? err : "decode failed");
        return false;
    }

    taskENTER_CRITICAL(&s_mux);
    uint16_t *stale = s_slot[slot].pending;   // unclaimed previous fetch, if any
    s_slot[slot].pending = panel;
    s_slot[slot].pend_w  = s_panel_w;
    s_slot[slot].pend_h  = s_panel_h;
    taskEXIT_CRITICAL(&s_mux);
    if (stale) heap_caps_free(stale);
    return true;
}

// Fetch one slot's URL, expanding {w}/{h} first. Returns false on any failure;
// the status line already carries the reason.
static bool fetch_one(const char *raw_url, int slot)
{
    char url[URL_MAX];
    expand_url(raw_url, url, sizeof(url), s_panel_w, s_panel_h);
    ESP_LOGI(TAG, "slot %d: fetching %s", slot, url);
    return do_fetch(url, slot);
}

static void fetch_task(void *arg)
{
    (void)arg;
    const bool batch = (s_req_slot < 0);

    // Count the work up front so the UI pill can say "2/5" from the first tick.
    // A batch also carries the internet assets (knob artwork, …): they are part
    // of the same download window, so they are part of the same count.
    const app_settings_t *cfg = settings_get();
    s_prog_done  = 0;
    s_prog_total = 1;
    if (batch) {
        s_prog_total = 0;
        for (int i = 0; i < NET_WP_SLOTS; i++)
            if (cfg->display.wallpaper_url[i][0]) s_prog_total++;
        s_prog_total += net_asset_url_count();
    }

    if (s_start_cb) s_start_cb();            // UI pill: explain the coming silence

    // Stop a playing radio stream for the duration: its (possibly TLS) socket
    // plus ours is exactly the two-HTTPS-sessions pattern that used to starve
    // internal RAM. The whole batch runs inside ONE stop/restore window — ten
    // slots must not mean ten interruptions of the music.
    app_state_t *st = app_state_get();
    bool was_radio = (st->radio_state == RADIO_STATE_PLAYING ||
                      st->radio_state == RADIO_STATE_BUFFERING);
    int  prev_idx  = st->curr_index;
    if (was_radio) radio_stop();

    bool ok = true;
    if (batch) {
        for (int i = 0; i < NET_WP_SLOTS; i++) {
            const char *u = cfg->display.wallpaper_url[i];
            if (!u[0]) continue;
            // One bad URL must not abandon the slots after it — remember the
            // failure, keep going, and report the batch as failed at the end.
            if (!fetch_one(u, i)) ok = false;
            s_prog_done++;
        }
        // Second phase: the internet assets. Same task, same stopped radio —
        // artwork the screens reference must not cost the music a second gap.
        for (int i = 0; i < NET_ASSET_SLOTS; i++) {
            const char *u = cfg->display.asset_url[i];
            if (!u[0]) continue;
            if (!net_asset_fetch_slot(i, u)) ok = false;
            s_prog_done++;
        }
    } else {
        ok = fetch_one(s_url, s_req_slot);
        s_prog_done = 1;
    }

    if (was_radio) radio_play_index(prev_idx);

    if (ok) net_fetch_set_status("ok");      // errors were set where they occurred
    s_busy = false;
    if (s_done_cb) s_done_cb(ok);
    net_wallpaper_sched_fetch_done(ok);      // scheduler retry/re-arm hook
    vTaskDelete(NULL);
}

// Shared tail of fetch()/fetch_all(): spawn the worker with an internal-RAM
// stack (the TLS handshake runs on it via esp_http_client, and mbedtls needs a
// few KB of headroom).
static bool start_fetch_task(void)
{
    net_fetch_set_status("busy");
    if (xTaskCreate(fetch_task, "net_wp", 10240, NULL, 5, NULL) != pdPASS) {
        net_fetch_set_err("task create failed");
        s_busy = false;
        return false;
    }
    return true;
}

// ── Public API ───────────────────────────────────────────────────────────────
bool net_wallpaper_fetch(int slot, const char *url, int panel_w, int panel_h)
{
    if (!slot_valid(slot) || !url || !url[0] || panel_w <= 0 || panel_h <= 0) return false;
    if (s_busy) return false;
    s_busy = true;

    strncpy(s_url, url, sizeof(s_url) - 1);
    s_url[sizeof(s_url) - 1] = '\0';
    s_req_slot = slot;
    s_panel_w  = panel_w;
    s_panel_h  = panel_h;

    return start_fetch_task();
}

bool net_wallpaper_fetch_all(int panel_w, int panel_h)
{
    if (panel_w <= 0 || panel_h <= 0) return false;
    if (s_busy) return false;

    // Nothing configured is success, not a failure: the scheduler would
    // otherwise burn its retry budget on a device with no URLs set. An asset URL
    // on its own is enough to make the batch worth running.
    const app_settings_t *cfg = settings_get();
    bool any = (net_asset_url_count() > 0);
    for (int i = 0; !any && i < NET_WP_SLOTS; i++)
        if (cfg->display.wallpaper_url[i][0]) any = true;
    if (!any) return false;

    s_busy     = true;
    s_req_slot = -1;
    s_panel_w  = panel_w;
    s_panel_h  = panel_h;

    return start_fetch_task();
}

const char *net_wallpaper_status(void)
{
    return net_fetch_status();
}

void net_wallpaper_progress(int *done, int *total)
{
    if (done)  *done  = s_prog_done;
    if (total) *total = s_prog_total;
}

const lv_image_dsc_t *net_wallpaper_image(int slot)
{
    if (!slot_valid(slot)) return NULL;
    return s_slot[slot].have ? &s_slot[slot].img : NULL;
}

bool net_wallpaper_any_image(void)
{
    for (int i = 0; i < NET_WP_SLOTS; i++)
        if (s_slot[i].have) return true;
    return false;
}

void net_wallpaper_commit(void)
{
    for (int i = 0; i < NET_WP_SLOTS; i++) {
        wp_slot_t *s = &s_slot[i];

        taskENTER_CRITICAL(&s_mux);
        uint16_t *p = s->pending;
        int w = s->pend_w, h = s->pend_h;
        bool dismiss = s->dismiss;
        s->pending = NULL;
        s->dismiss = false;
        taskEXIT_CRITICAL(&s_mux);

        // An explicit background choice outranks everything, including a fetch
        // that happened to finish in the meantime — drop both image and pending.
        if (dismiss) {
            if (p) heap_caps_free(p);
            xSemaphoreTake(buf_lock(), portMAX_DELAY);
            if (s->have) {
                lv_image_cache_drop(&s->img);
                s->have = false;
            }
            if (s->buf) {
                heap_caps_free(s->buf);
                s->buf = NULL;
            }
            xSemaphoreGive(s_buf_lock);
            ESP_LOGI(TAG, "slot %d dismissed", i);
            continue;
        }
        if (!p) continue;

        xSemaphoreTake(buf_lock(), portMAX_DELAY);
        if (s->have) lv_image_cache_drop(&s->img);
        if (s->buf) heap_caps_free(s->buf);
        s->buf = p;

        s->img.header.magic  = LV_IMAGE_HEADER_MAGIC;
        s->img.header.cf     = LV_COLOR_FORMAT_RGB565;
        s->img.header.w      = w;
        s->img.header.h      = h;
        s->img.header.stride = w * 2;
        s->img.data_size     = (uint32_t)w * h * 2;
        s->img.data          = (const uint8_t *)p;
        s->have = true;
        xSemaphoreGive(s_buf_lock);
        ESP_LOGI(TAG, "slot %d committed (%dx%d)", i, w, h);
    }
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

// A saved wallpaper is panel-sized, so it is filed per resolution the same way
// the web layout editor files its uploads and layout presets, in a subfolder
// naming its origin: /sdcard/wallpapers/<width>x<height>/internet/.
#define WALLPAPER_ROOT "/sdcard/wallpapers"

bool net_wallpaper_save_to_sd(int slot, char *out_path, size_t out_cap, const char **err)
{
    *err = NULL;
    if (!slot_valid(slot)) { *err = "bad slot"; return false; }
    // Also closes the buffer-swap race: a new image can only be committed at
    // the tail of a fetch, and no fetch is running past this check.
    if (s_busy) { *err = "fetch in progress"; return false; }
    if (sdcard_init() != ESP_OK || !sdcard_is_mounted()) { *err = "no SD card"; return false; }

    // Snapshot under the buffer lock (one fast memcpy), then write the file
    // from the copy with no lock held — the SD write takes ~a second.
    xSemaphoreTake(buf_lock(), portMAX_DELAY);
    if (!s_slot[slot].have) {
        xSemaphoreGive(s_buf_lock);
        *err = "no wallpaper fetched";
        return false;
    }
    const int w = s_slot[slot].img.header.w, h = s_slot[slot].img.header.h;
    const size_t bytes = (size_t)w * h * 2;
    uint16_t *copy = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
    if (copy) memcpy(copy, s_slot[slot].buf, bytes);
    xSemaphoreGive(s_buf_lock);
    if (!copy) { *err = "no PSRAM"; return false; }

    char res_dir[48], save_dir[64];
    snprintf(res_dir, sizeof(res_dir), WALLPAPER_ROOT "/%dx%d", w, h);
    snprintf(save_dir, sizeof(save_dir), "%s/internet", res_dir);
    mkdir(WALLPAPER_ROOT, 0775);   // EEXIST is fine for every level
    mkdir(res_dir, 0775);
    mkdir(save_dir, 0775);

    time_t now = time(NULL);
    struct tm lt;
    localtime_r(&now, &lt);
    if (lt.tm_year + 1900 >= 2020) {
        snprintf(out_path, out_cap, "%s/net_%04d%02d%02d_%02d%02d%02d.bin", save_dir,
                 lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday,
                 lt.tm_hour, lt.tm_min, lt.tm_sec);
    } else {   // clock not NTP-synced yet — fall back to a boot-unique stamp
        snprintf(out_path, out_cap, "%s/net_%08lx.bin", save_dir,
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

uint8_t *net_wallpaper_bin_snapshot(int slot, size_t *out_len)
{
    if (!slot_valid(slot)) return NULL;
    xSemaphoreTake(buf_lock(), portMAX_DELAY);
    if (!s_slot[slot].have) {
        xSemaphoreGive(s_buf_lock);
        return NULL;
    }
    const int w = s_slot[slot].img.header.w, h = s_slot[slot].img.header.h;
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
        memcpy(bin + sizeof(hdr), s_slot[slot].buf, bytes);
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

void net_wallpaper_dismiss(int slot)
{
    taskENTER_CRITICAL(&s_mux);
    if (slot < 0) {
        for (int i = 0; i < NET_WP_SLOTS; i++) s_slot[i].dismiss = true;
    } else if (slot < NET_WP_SLOTS) {
        s_slot[slot].dismiss = true;
    }
    taskEXIT_CRITICAL(&s_mux);
}
