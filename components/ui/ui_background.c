#include "ui_background.h"
#include "ui_profile.h"        // DISPLAY_WIDTH / DISPLAY_HEIGHT
#include "theme.h"             // ui_theme_t, theme_current(), theme_get()
#include "settings.h"          // settings_get()->display.bg_gradient
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <stdint.h>
#include <stdbool.h>

static const char *TAG = "UI_BG";

#if !defined(UI_PROFILE_MONO_128X64) && !defined(UI_PROFILE_MONO_256X64)

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

void ui_background_apply(lv_obj_t *obj)
{
    const ui_theme_t t = theme_current();

    // Solid background (gradient disabled): the pre-gradient look, theme-aware.
    if (!settings_get()->display.bg_gradient) {
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

void ui_background_apply(lv_obj_t *obj) { (void)obj; }

#endif
