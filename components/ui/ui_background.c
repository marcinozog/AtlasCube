#include "ui_background.h"
#include "ui_profile.h"        // DISPLAY_WIDTH / DISPLAY_HEIGHT
#include "theme.h"             // ui_theme_t, theme_current(), theme_get()
#include "settings.h"          // settings_get()->display.bg_gradient
#include "sdcard.h"            // SD_MOUNT_POINT
#include "net_wallpaper.h"     // internet-fetched wallpaper (PSRAM only)
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "UI_BG";

#if !defined(UI_PROFILE_MONO_128X64) && !defined(UI_PROFILE_MONO_256X64)

// ── Wallpaper ───────────────────────────────────────────────────────────────
// Full-screen LVGL .bin (RGB565, DISPLAY_WIDTH x DISPLAY_HEIGHT) backgrounds
// loaded from SD, used in place of the gradient when display.wallpaper_on is
// set. The path is resolved per screen: the hub sections in ui_profile
// (clock/radio/sd/bt) may override the global default (display.wallpaper_path)
// with their own file, or opt out with "none". Every distinct path gets its own
// PSRAM slot so navigating between screens never re-reads the SD card (which
// would contend with SD music playback). Same .bin format the photo screensaver
// consumes — see scripts/img2lvgl.py.
#define LV_BIN_MAGIC     0x19
#define LV_BIN_RGB565    0x12
#define WP_SLOTS         5   // 4 per-screen overrides + the global default

typedef struct __attribute__((packed)) {
    uint8_t  magic;
    uint8_t  cf;
    uint16_t flags;
    uint16_t w;
    uint16_t h;
    uint16_t stride;
    uint16_t reserved;
} bin_header_t;

typedef struct {
    char           path[128];  // resolved fopen path ("" → slot unused)
    uint16_t      *buf;        // panel-sized PSRAM buffer, reused across reloads
    lv_image_dsc_t img;
    bool           loaded;     // buf holds a valid image of `path`
    bool           tried;      // load attempted — a failure latches until reload
} wp_slot_t;

static wp_slot_t s_wp[WP_SLOTS];
static int       s_wp_evict = 0;   // round-robin victim when every slot is taken

// Descriptor to display for the internet wallpaper: the pristine original when
// dim is off, otherwise a dimmed PSRAM copy (built in net_dimmed()), rebuilt
// when the source image or the dim value changes. net_wallpaper's own buffer is
// never touched, so save-to-SD always writes the original and dim stays fully
// reversible.
static uint16_t      *s_net_dim_buf = NULL;
static lv_image_dsc_t s_net_dim_img;
static const uint8_t *s_net_dim_src = NULL;   // source data of the current copy
static int            s_net_dim_pct = -1;

// Darken an RGB565 buffer in place by dim_pct percent (0-80). Applied once at
// load/copy time so the displayed wallpaper carries the dim baked in — no
// per-frame blending. Sources (the SD file, net_wallpaper's buffer) stay
// pristine, so save-to-SD and a later lower dim never lose brightness.
static void dim_rgb565(uint16_t *px, size_t n, int dim_pct)
{
    if (dim_pct <= 0) return;
    const uint32_t f = (uint32_t)(100 - dim_pct) * 256 / 100;   // 8.8 fixed point
    for (size_t i = 0; i < n; i++) {
        uint16_t c = px[i];
        uint16_t r = (uint16_t)((((c >> 11) & 0x1F) * f) >> 8);
        uint16_t g = (uint16_t)((((c >> 5)  & 0x3F) * f) >> 8);
        uint16_t b = (uint16_t)((( c        & 0x1F) * f) >> 8);
        px[i] = (uint16_t)((r << 11) | (g << 5) | b);
    }
}

// Per-screen wallpaper override from ui_profile: "" inherit, "none" opt out,
// else an fopen path. Returns NULL for screens without a hub section — they
// always follow the global default.
static const char *screen_wp_override(ui_screen_id_t screen)
{
    const ui_profile_t *p = ui_profile_get();
    switch (screen) {
        case SCREEN_HOME:  return p->clock_wallpaper;
        case SCREEN_RADIO: return p->radio_wallpaper;
        case SCREEN_SD:    return p->sd_wallpaper;
        case SCREEN_BT:    return p->bt_wallpaper;
        default:           return NULL;
    }
}

// Find (or claim) the cache slot for `path`. Prefers an existing match, then an
// unused slot; when all are taken, evicts round-robin — with WP_SLOTS above the
// number of screens that can override, eviction only happens after the user has
// cycled through many distinct files in one session.
static wp_slot_t *wp_slot_for(const char *path)
{
    wp_slot_t *victim = NULL;
    for (int i = 0; i < WP_SLOTS; i++) {
        if (strcmp(s_wp[i].path, path) == 0) return &s_wp[i];
        if (!victim && !s_wp[i].path[0]) victim = &s_wp[i];
    }
    if (!victim) {
        victim = &s_wp[s_wp_evict];
        s_wp_evict = (s_wp_evict + 1) % WP_SLOTS;
    }
    if (victim->loaded) lv_image_cache_drop(&victim->img);
    strncpy(victim->path, path, sizeof(victim->path) - 1);
    victim->path[sizeof(victim->path) - 1] = '\0';
    victim->loaded = false;
    victim->tried  = false;
    return victim;
}

// Read the slot's .bin into its PSRAM buffer once. Returns true if a valid,
// correctly sized image is now in s->img. Failures (no file / wrong size) are
// latched and never retried until ui_background_reload_wallpaper() clears the
// latch, so apply() falls back to the gradient meanwhile.
static bool wp_slot_load(wp_slot_t *s)
{
    if (s->tried) return s->loaded;

    // ui_background_apply() runs very early (before the splash), so the SD card
    // may not be mounted yet — mount it lazily. Until it's up, leave s->tried
    // unset so a later re-apply (e.g. UI_EVT_BG_CHANGED) retries.
    if (sdcard_init() != ESP_OK || !sdcard_is_mounted()) {
        ESP_LOGI(TAG, "SD not ready — wallpaper deferred");
        return false;
    }
    s->tried = true;

    const int W = DISPLAY_WIDTH, H = DISPLAY_HEIGHT;
    FILE *fp = fopen(s->path, "rb");
    if (!fp) { ESP_LOGI(TAG, "no wallpaper at %s", s->path); return false; }

    bin_header_t h;
    if (fread(&h, sizeof(h), 1, fp) != 1 ||
        h.magic != LV_BIN_MAGIC || h.cf != LV_BIN_RGB565 ||
        h.w != W || h.h != H) {
        ESP_LOGW(TAG, "wallpaper bad header or size %ux%u (need %dx%d)", h.w, h.h, W, H);
        fclose(fp);
        return false;
    }

    if (!s->buf) {
        s->buf = heap_caps_malloc((size_t)W * H * 2, MALLOC_CAP_SPIRAM);
        if (!s->buf) { ESP_LOGE(TAG, "wallpaper buffer alloc failed"); fclose(fp); return false; }
    }
    size_t got = fread(s->buf, (size_t)W * 2, H, fp);
    fclose(fp);
    if (got != (size_t)H) { ESP_LOGW(TAG, "wallpaper short read (%u/%d rows)", (unsigned)got, H); return false; }

    // Dim changes ride UI_EVT_BG_CHANGED → reload_wallpaper() → fresh re-read
    // from the file, so dimming in place here is always applied exactly once.
    dim_rgb565(s->buf, (size_t)W * H, settings_get()->display.wallpaper_dim);

    s->img.header.magic  = LV_IMAGE_HEADER_MAGIC;
    s->img.header.cf     = LV_COLOR_FORMAT_RGB565;
    s->img.header.w      = W;
    s->img.header.h      = H;
    s->img.header.stride = W * 2;
    s->img.data_size     = (uint32_t)(W * H * 2);
    s->img.data          = (const uint8_t *)s->buf;
    s->loaded = true;
    ESP_LOGI(TAG, "wallpaper loaded from %s", s->path);
    return true;
}

// Forget every loaded wallpaper so the next ui_background_apply() re-reads from
// SD (fresh file content, dim re-baked). Must run on the LVGL task (drops the
// image cache for reused descriptors); UI_EVT_BG_CHANGED is dispatched there.
// Buffers stay allocated — the next load reuses the same panel-sized blocks.
void ui_background_reload_wallpaper(void)
{
    net_wallpaper_commit();   // adopt a finished internet fetch, if any (LVGL task)
    for (int i = 0; i < WP_SLOTS; i++) {
        if (s_wp[i].loaded) lv_image_cache_drop(&s_wp[i].img);
        s_wp[i].path[0] = '\0';
        s_wp[i].loaded  = false;
        s_wp[i].tried   = false;
    }
    // Invalidate the dimmed net-wallpaper copy: a new fetch may land at the
    // same PSRAM address, so the data pointer alone can't be trusted as a key.
    s_net_dim_src = NULL;
}

// Vertical gradient endpoints (top → bottom). The colours live in the per-theme
// palette (ui_theme_colors_t.bg_grad_top/bottom), editable from the web UI.
typedef struct { uint8_t r, g, b; } bg_rgb_t;

static inline bg_rgb_t unpack(uint32_t c)
{
    return (bg_rgb_t){ (c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF };
}

static void theme_gradient(ui_theme_t t, bg_rgb_t *top, bg_rgb_t *bot)
{
    const ui_theme_colors_t *c = theme_palette_get(t);
    *top = unpack(c->bg_grad_top);
    *bot = unpack(c->bg_grad_bottom);
}

// One buffer + descriptor per theme (index 0 = dark, 1 = light). Built lazily and
// rebuilt only when its palette colours change, so LVGL's image cache (keyed by
// descriptor pointer) never serves a stale gradient.
static uint16_t      *s_buf[2];
static lv_image_dsc_t s_img[2];
static bool           s_built[2];
static uint32_t       s_built_top[2];   // palette colours the buffer was rendered with
static uint32_t       s_built_bot[2];

static inline int theme_idx(ui_theme_t t) { return (t == THEME_LIGHT) ? 1 : 0; }

// Rendered once into a PSRAM buffer with ordered (Bayer 8x8) dithering. LVGL 9
// dropped software gradient dithering (issue #6696, "not planned"), so we do it
// ourselves: the dither adds +/- half of one RGB565 quantization step before
// rounding, scattering the otherwise-banded boundaries into fine grain. A pure
// black bottom (dark theme) stays clean — the offset can't push 0 above step 1.
static void build_bg_image(int idx, ui_theme_t t)
{
    static const uint8_t bayer[8][8] = {
        {  0, 32,  8, 40,  2, 34, 10, 42 },
        { 48, 16, 56, 24, 50, 18, 58, 26 },
        { 12, 44,  4, 36, 14, 46,  6, 38 },
        { 60, 28, 52, 20, 62, 30, 54, 22 },
        {  3, 35, 11, 43,  1, 33,  9, 41 },
        { 51, 19, 59, 27, 49, 17, 57, 25 },
        { 15, 47,  7, 39, 13, 45,  5, 37 },
        { 63, 31, 55, 23, 61, 29, 53, 21 },
    };
    const int W = DISPLAY_WIDTH, H = DISPLAY_HEIGHT;
    const int Hm1 = (H > 1) ? (H - 1) : 1;

    if (!s_buf[idx]) {
        s_buf[idx] = heap_caps_malloc((size_t)W * H * 2, MALLOC_CAP_SPIRAM);
        if (!s_buf[idx]) { ESP_LOGE(TAG, "bg buffer alloc failed (%d B)", W * H * 2); return; }
    }

    bg_rgb_t top, bot;
    theme_gradient(t, &top, &bot);

    for (int y = 0; y < H; y++) {
        // Linear top→bottom interpolation per channel.
        int vr = top.r + (bot.r - top.r) * y / Hm1;
        int vg = top.g + (bot.g - top.g) * y / Hm1;
        int vb = top.b + (bot.b - top.b) * y / Hm1;
        for (int x = 0; x < W; x++) {
            int d = (int)bayer[y & 7][x & 7] - 32;   // -32..31
            int tr = vr + (d * 8) / 64;              // +/- half of the 5-bit step (8)
            int tg = vg + (d * 4) / 64;              // +/- half of the 6-bit step (4)
            int tb = vb + (d * 8) / 64;
            if (tr < 0) tr = 0; else if (tr > 255) tr = 255;
            if (tg < 0) tg = 0; else if (tg > 255) tg = 255;
            if (tb < 0) tb = 0; else if (tb > 255) tb = 255;
            s_buf[idx][y * W + x] =
                (uint16_t)(((tr >> 3) << 11) | ((tg >> 2) << 5) | (tb >> 3));
        }
    }

    s_img[idx].header.magic  = LV_IMAGE_HEADER_MAGIC;
    s_img[idx].header.cf     = LV_COLOR_FORMAT_RGB565;
    s_img[idx].header.w      = W;
    s_img[idx].header.h      = H;
    s_img[idx].header.stride = W * 2;
    s_img[idx].data_size     = (uint32_t)(W * H * 2);
    s_img[idx].data          = (const uint8_t *)s_buf[idx];

    const ui_theme_colors_t *c = theme_palette_get(t);
    s_built_top[idx] = c->bg_grad_top;
    s_built_bot[idx] = c->bg_grad_bottom;
    s_built[idx]     = true;
}

static const lv_image_dsc_t *net_dimmed(const lv_image_dsc_t *src)
{
    const int dim = settings_get()->display.wallpaper_dim;
    if (dim <= 0) return src;
    // The fetch path decodes to exactly the panel size; anything else would
    // also break the bg fit, so just show it undimmed rather than overrun.
    if (src->header.w != DISPLAY_WIDTH || src->header.h != DISPLAY_HEIGHT) return src;
    if (s_net_dim_src == src->data && s_net_dim_pct == dim) return &s_net_dim_img;

    const size_t n = (size_t)DISPLAY_WIDTH * DISPLAY_HEIGHT;
    if (!s_net_dim_buf) {
        s_net_dim_buf = heap_caps_malloc(n * 2, MALLOC_CAP_SPIRAM);
        if (!s_net_dim_buf) { ESP_LOGE(TAG, "net dim buffer alloc failed"); return src; }
    }
    lv_image_cache_drop(&s_net_dim_img);
    memcpy(s_net_dim_buf, src->data, n * 2);
    dim_rgb565(s_net_dim_buf, n, dim);
    s_net_dim_img      = *src;
    s_net_dim_img.data = (const uint8_t *)s_net_dim_buf;
    s_net_dim_src      = src->data;
    s_net_dim_pct      = dim;
    return &s_net_dim_img;
}

void ui_background_apply(lv_obj_t *obj, ui_screen_id_t screen)
{
    const ui_theme_t t = theme_current();
    const app_settings_t *st = settings_get();

    // Per-screen override — honoured only while the global feature switch is
    // on (wallpaper_on gates every SD wallpaper, overrides included).
    const char *ovr = st->display.wallpaper_on ? screen_wp_override(screen) : NULL;
    if (ovr && !ovr[0]) ovr = NULL;   // "" → inherit the global default
    const bool ovr_none = ovr && strcmp(ovr, "none") == 0;

    // Internet wallpaper (PSRAM only, via /api/wallpaper/fetch): an explicit
    // user fetch that temporarily replaces the GLOBAL wallpaper until the next
    // reboot. It substitutes only the inherited tier — a screen with its own
    // override ("none" or a path) keeps its explicit choice.
    if (!ovr) {
        const lv_image_dsc_t *net_wp = net_wallpaper_image();
        if (net_wp) {
            lv_obj_set_style_bg_image_src(obj, net_dimmed(net_wp), LV_PART_MAIN);
            lv_obj_set_style_bg_image_tiled(obj, false, LV_PART_MAIN);
            lv_obj_set_style_bg_image_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
            return;
        }
    }

    // SD wallpaper for this screen (override path, else the global default):
    // a valid full-screen .bin wins over the gradient entirely. A "none"
    // override or a failed load falls through to the gradient/solid below.
    const char *wp_path = NULL;
    if (!ovr_none) {
        if (ovr)
            wp_path = ovr;
        else if (st->display.wallpaper_on && st->display.wallpaper_path[0])
            wp_path = st->display.wallpaper_path;
    }
    if (wp_path) {
        wp_slot_t *slot = wp_slot_for(wp_path);
        if (wp_slot_load(slot)) {
            lv_obj_set_style_bg_image_src(obj, &slot->img, LV_PART_MAIN);
            lv_obj_set_style_bg_image_tiled(obj, false, LV_PART_MAIN);
            lv_obj_set_style_bg_image_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
            return;
        }
    }

    // Solid background (gradient disabled): the pre-gradient look, theme-aware.
    if (!st->display.bg_gradient) {
        lv_obj_set_style_bg_image_src(obj, NULL, LV_PART_MAIN);
        lv_obj_set_style_bg_color(obj, lv_color_hex(theme_get()->bg_primary), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
        return;
    }

    const int idx = theme_idx(t);
    const ui_theme_colors_t *c = theme_palette_get(t);
    if (!s_built[idx] || s_built_top[idx] != c->bg_grad_top || s_built_bot[idx] != c->bg_grad_bottom)
        build_bg_image(idx, t);

    bg_rgb_t top, bot;
    theme_gradient(t, &top, &bot);
    lv_obj_set_style_bg_color(obj, lv_color_make(top.r, top.g, top.b), LV_PART_MAIN);  // fallback
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);

    if (s_built[idx]) {
        lv_obj_set_style_bg_image_src(obj, &s_img[idx], LV_PART_MAIN);
        lv_obj_set_style_bg_image_tiled(obj, false, LV_PART_MAIN);
        lv_obj_set_style_bg_image_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    }
}

#else  // mono panel — no gradient background

void ui_background_apply(lv_obj_t *obj, ui_screen_id_t screen) { (void)obj; (void)screen; }
void ui_background_reload_wallpaper(void) { }

#endif
