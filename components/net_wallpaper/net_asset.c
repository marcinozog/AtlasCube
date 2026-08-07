#include "net_asset.h"
#include "net_fetch.h"
#include "settings.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "libs/lodepng/lodepng.h"
#include <string.h>

#if !LV_USE_LODEPNG
#error "net_asset needs LVGL's PNG decoder — set CONFIG_LV_USE_LODEPNG=y"
#endif

static const char *TAG = "NET_ASSET";

#define PNG_MAX  (256 * 1024)   // download cap — knob artwork is a few KB

// One independent asset per slot. `png` is the fetch task's handoff (raw
// downloaded bytes); `buf`/`img` are the decoded artwork, owned by the LVGL task.
// No mutex over the decoded side: everything that reads or replaces it —
// commit, net_asset_image(), the widgets — runs on the LVGL task. Only the
// handoff pointer is touched from two tasks, and a spinlock covers that.
typedef struct {
    uint8_t       *buf;       // RGB565 plane followed by the A8 plane, PSRAM
    lv_image_dsc_t img;
    bool           have;
    uint8_t       *png;       // downloaded, waiting to be decoded
    int            png_len;
} asset_slot_t;

static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static asset_slot_t s_slot[NET_ASSET_SLOTS];

static inline bool slot_valid(int slot) { return slot >= 0 && slot < NET_ASSET_SLOTS; }

// ── Fetch task side ──────────────────────────────────────────────────────────

// Read width/height straight out of the IHDR chunk, which a valid PNG always
// puts first: bytes 0-7 signature, 8-11 length, 12-15 "IHDR", 16-23 w/h as
// big-endian u32. Hand-parsed rather than via lodepng_inspect() because that
// allocates through lv_malloc, which this task must not touch — and it lets a
// bad or oversized URL fail here, where the error can still name the slot,
// instead of much later on the LVGL task.
static bool png_dimensions(const uint8_t *png, int len, unsigned *w, unsigned *h)
{
    static const uint8_t sig[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
    if (len < 24 || memcmp(png, sig, sizeof(sig)) != 0 || memcmp(png + 12, "IHDR", 4) != 0)
        return false;
    *w = ((unsigned)png[16] << 24) | ((unsigned)png[17] << 16) |
         ((unsigned)png[18] << 8)  |  (unsigned)png[19];
    *h = ((unsigned)png[20] << 24) | ((unsigned)png[21] << 16) |
         ((unsigned)png[22] << 8)  |  (unsigned)png[23];
    return *w > 0 && *h > 0;
}

bool net_asset_fetch_slot(int slot, const char *url)
{
    if (!slot_valid(slot) || !url || !url[0]) return false;

    ESP_LOGI(TAG, "slot %d: fetching %s", slot, url);
    int len = 0;
    uint8_t *png = net_fetch_download(url, PNG_MAX, &len);
    if (!png) return false;

    unsigned w = 0, h = 0;
    if (!png_dimensions(png, len, &w, &h)) {
        heap_caps_free(png);
        net_fetch_set_err("asset %d: not a PNG", slot);
        return false;
    }
    if ((size_t)w * h > NET_ASSET_MAX_PX) {
        heap_caps_free(png);
        net_fetch_set_err("asset %d: %ux%u over the %d px limit", slot, w, h, NET_ASSET_MAX_PX);
        return false;
    }
    ESP_LOGI(TAG, "slot %d: downloaded %d B, %ux%u", slot, len, w, h);

    taskENTER_CRITICAL(&s_mux);
    uint8_t *stale = s_slot[slot].png;      // unclaimed previous fetch, if any
    s_slot[slot].png     = png;
    s_slot[slot].png_len = len;
    taskEXIT_CRITICAL(&s_mux);
    if (stale) heap_caps_free(stale);
    return true;
}

int net_asset_url_count(void)
{
    const app_settings_t *cfg = settings_get();
    int n = 0;
    for (int i = 0; i < NET_ASSET_SLOTS; i++)
        if (cfg->display.asset_url[i][0]) n++;
    return n;
}

// ── LVGL task side ───────────────────────────────────────────────────────────

// Decode a PNG into a fresh PSRAM buffer holding LVGL's RGB565A8 layout: the
// full RGB565 plane (stride w*2) followed by the A8 plane (stride w). Returns
// NULL with the status line set on any failure.
static uint8_t *decode_rgb565a8(const uint8_t *png, int len, int *out_w, int *out_h)
{
    unsigned w = 0, h = 0;
    unsigned char *decoded = NULL;
    const unsigned err = lodepng_decode32(&decoded, &w, &h, png, (size_t)len);

    // LVGL's vendored lodepng is patched: lodepng_decode() hands back an
    // lv_draw_buf_t (ARGB8888, stride 4*w), NOT the bare pixel array upstream
    // lodepng returns. So the pixels live at ->data and the buffer is released
    // with lv_draw_buf_destroy() — lv_free() on it would corrupt the pool.
    lv_draw_buf_t *db = (lv_draw_buf_t *)decoded;
    if (err || !db) {
        net_fetch_set_err("png decode: %s", err ? lodepng_error_text(err) : "no output");
        if (db) lv_draw_buf_destroy(db);
        return NULL;
    }

    const size_t n = (size_t)w * h;
    uint8_t *out = heap_caps_malloc(n * 3, MALLOC_CAP_SPIRAM);
    if (!out) {
        lv_draw_buf_destroy(db);
        net_fetch_set_err("no PSRAM for %ux%u asset", w, h);
        return NULL;
    }

    // lodepng wrote plain RGBA bytes (we asked for LCT_RGBA/8 and did not run
    // LVGL's R↔B swap), so the channel order here is R,G,B,A.
    uint16_t      *rgb = (uint16_t *)out;
    uint8_t       *a   = out + n * 2;
    const uint8_t *src = db->data;
    for (size_t i = 0; i < n; i++, src += 4) {
        rgb[i] = (uint16_t)(((src[0] >> 3) << 11) |
                            ((src[1] >> 2) << 5)  |
                             (src[2] >> 3));
        a[i]   = src[3];
    }

    lv_draw_buf_destroy(db);
    *out_w = (int)w;
    *out_h = (int)h;
    return out;
}

bool net_asset_commit(void)
{
    bool changed = false;

    for (int i = 0; i < NET_ASSET_SLOTS; i++) {
        asset_slot_t *s = &s_slot[i];

        taskENTER_CRITICAL(&s_mux);
        uint8_t *png = s->png;
        int      len = s->png_len;
        s->png     = NULL;
        s->png_len = 0;
        taskEXIT_CRITICAL(&s_mux);
        if (!png) continue;

        int w = 0, h = 0;
        uint8_t *pix = decode_rgb565a8(png, len, &w, &h);
        heap_caps_free(png);
        if (!pix) continue;   // keep whatever the slot already had

        if (s->have) lv_image_cache_drop(&s->img);
        if (s->buf)  heap_caps_free(s->buf);
        s->buf = pix;

        s->img.header.magic  = LV_IMAGE_HEADER_MAGIC;
        s->img.header.cf     = LV_COLOR_FORMAT_RGB565A8;
        s->img.header.w      = (uint32_t)w;
        s->img.header.h      = (uint32_t)h;
        s->img.header.stride = (uint32_t)w * 2;   // colour plane; LVGL takes the
                                                  // alpha stride as half of this
        s->img.data_size     = (uint32_t)w * h * 3;
        s->img.data          = pix;
        s->have   = true;
        changed   = true;
        ESP_LOGI(TAG, "slot %d decoded (%dx%d)", i, w, h);
    }
    return changed;
}

const lv_image_dsc_t *net_asset_image(int slot)
{
    if (!slot_valid(slot)) return NULL;
    return s_slot[slot].have ? &s_slot[slot].img : NULL;
}

bool net_asset_filled(int slot)
{
    return slot_valid(slot) && s_slot[slot].have;
}
