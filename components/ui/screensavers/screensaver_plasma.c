#include "screensaver_plasma.h"
#include "ui_screen.h"
#include "ui_profile.h"
#include "lvgl.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <string.h>
#include <math.h>

static const char *TAG = "SS_PLASMA";

#define FRAME_MS      33      // ~30 fps
#define LUT_SIZE      256
#define PALETTE_SIZE  256

static lv_obj_t   *s_root    = NULL;
static lv_obj_t   *s_canvas  = NULL;
static uint16_t   *s_buf     = NULL;
static lv_timer_t *s_timer   = NULL;
static int         s_w = 0, s_h = 0;
static uint32_t    s_t       = 0;

// LUT in BSS — built once on first create
static int8_t      s_sin_lut[LUT_SIZE];        // sin → [-127, 127]
static uint16_t    s_palette[PALETTE_SIZE];    // RGB565 rainbow
static bool        s_lut_ready = false;

// ---------------------------------------------------------------------------
// table init
// ---------------------------------------------------------------------------

// Palette = linear interpolation between three RGB anchors.
// Tweak these to change the mood — keep all components ≤ ~150 to stay
// AMOLED-friendly and avoid washing into "pink".
static const uint8_t anchor_low [3] = {  18,  30, 110 };  // deep blue
static const uint8_t anchor_mid [3] = {  70,  20, 105 };  // dim indigo/violet (less pink)
static const uint8_t anchor_high[3] = { 130,  20,  35 };  // deep red

static inline uint8_t lerp_u8(uint8_t a, uint8_t b, int k, int range)
{
    return (uint8_t)((int)a + (((int)b - (int)a) * k) / range);
}

static void init_tables(void)
{
    if (s_lut_ready) return;

    for (int i = 0; i < LUT_SIZE; i++) {
        s_sin_lut[i] = (int8_t)(sinf((float)i * 2.0f * 3.14159265f / LUT_SIZE) * 127.0f);
    }
    for (int i = 0; i < PALETTE_SIZE; i++) {
        uint8_t r, g, b;
        if (i < 128) {
            r = lerp_u8(anchor_low[0],  anchor_mid[0],  i, 127);
            g = lerp_u8(anchor_low[1],  anchor_mid[1],  i, 127);
            b = lerp_u8(anchor_low[2],  anchor_mid[2],  i, 127);
        } else {
            int k = i - 128;
            r = lerp_u8(anchor_mid[0],  anchor_high[0], k, 127);
            g = lerp_u8(anchor_mid[1],  anchor_high[1], k, 127);
            b = lerp_u8(anchor_mid[2],  anchor_high[2], k, 127);
        }
        s_palette[i] = (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
    }
    s_lut_ready = true;
}

// ---------------------------------------------------------------------------
// frame
// ---------------------------------------------------------------------------

static void frame_tick(lv_timer_t *t)
{
    (void)t;
    if (!s_buf || !s_canvas) return;

    s_t++;
    // Phase offsets — three different speeds give slow morphing
    const int t1 = (int)(s_t * 2) & 0xFF;
    const int t2 = (int)(s_t * 3) & 0xFF;
    const int t3 = (int)(s_t * 1) & 0xFF;

    uint16_t *p = s_buf;
    for (int y = 0; y < s_h; y++) {
        // Y-only terms hoisted out of the inner loop
        const int by  = (int)((unsigned)(y * 4 + t2) & 0xFFu);
        const int v2  = s_sin_lut[by];
        const int yt3 = (int)((unsigned)(y * 2 + t3) & 0xFFu);

        for (int x = 0; x < s_w; x++) {
            const int v1  = s_sin_lut[(unsigned)(x * 3 + t1) & 0xFFu];
            const int v3  = s_sin_lut[(unsigned)(x * 2 + yt3) & 0xFFu];
            const int sum = v1 + v2 + v3;             // [-381, 381]
            // map to [0, ~254] via *85>>8 (≈ /3)
            uint8_t c = (uint8_t)(((sum + 384) * 85) >> 8);
            *p++ = s_palette[c];
        }
    }

    lv_obj_invalidate(s_canvas);
}

// ---------------------------------------------------------------------------
// screen lifecycle
// ---------------------------------------------------------------------------

static void plasma_create(lv_obj_t *parent)
{
    s_w = DISPLAY_WIDTH;
    s_h = DISPLAY_HEIGHT;
    s_t = 0;

    init_tables();

    s_root = lv_obj_create(parent);
    lv_obj_remove_style_all(s_root);
    lv_obj_set_size(s_root, s_w, s_h);
    lv_obj_set_style_bg_color(s_root, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);

    size_t sz = (size_t)s_w * (size_t)s_h * 2;
    s_buf = heap_caps_aligned_alloc(4, sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_buf) {
        ESP_LOGE(TAG, "Canvas buffer alloc failed (%u bytes)", (unsigned)sz);
        return;
    }
    memset(s_buf, 0, sz);

    s_canvas = lv_canvas_create(s_root);
    lv_canvas_set_buffer(s_canvas, s_buf, s_w, s_h, LV_COLOR_FORMAT_RGB565);
    lv_obj_align(s_canvas, LV_ALIGN_TOP_LEFT, 0, 0);

    s_timer = lv_timer_create(frame_tick, FRAME_MS, NULL);

    ESP_LOGI(TAG, "Created (canvas %dx%d in PSRAM = %u B)", s_w, s_h, (unsigned)sz);
}

static void plasma_destroy(void)
{
    if (s_timer)  { lv_timer_delete(s_timer);  s_timer  = NULL; }
    if (s_canvas) { lv_obj_delete(s_canvas);   s_canvas = NULL; }
    if (s_root)   { lv_obj_delete(s_root);     s_root   = NULL; }
    if (s_buf)    { heap_caps_free(s_buf);     s_buf    = NULL; }
    ESP_LOGI(TAG, "Destroyed");
}

const ui_screen_t screensaver_plasma = {
    .create      = plasma_create,
    .destroy     = plasma_destroy,
    .apply_theme = NULL,
    .on_event    = NULL,
    .on_input    = NULL,
    .name        = "ss_plasma",
};
