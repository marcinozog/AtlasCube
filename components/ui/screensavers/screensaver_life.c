#include "screensaver_life.h"
#include "ui_screen.h"
#include "ui_profile.h"
#include "lvgl.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_random.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "SS_LIFE";

#define CELL_PX       7        // 6x6 lit pixels + 1 px gutter
#define FRAME_MS      150      // ~8 fps — slow enough to read evolution
#define STALE_LIMIT   30       // generations of repeating state before reseed
#define AGE_MAX       250
#define SEED_DENSITY  80       // out of 256 → ~31 % live at reseed

static lv_obj_t   *s_root   = NULL;
static lv_obj_t   *s_canvas = NULL;
static uint16_t   *s_buf    = NULL;
static lv_timer_t *s_timer  = NULL;
static int         s_w = 0, s_h = 0;
static int         s_cols = 0, s_rows = 0;
static int         s_offx = 0, s_offy = 0;
static uint8_t    *s_age   = NULL;   // age per cell, 0 = dead, n = alive for n gens
static uint8_t    *s_next  = NULL;   // next-gen alive bit (0/1)
static uint32_t    s_hist[4];        // hash history for stagnation detection
static uint16_t    s_stale = 0;

#define IDX(x, y) ((y) * s_cols + (x))

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

// Age → color. Bright cyan at birth, fading through green/yellow/orange to dim red.
static uint16_t color_for_age(uint8_t age)
{
    if (age == 1)  return rgb565(140, 240, 255);   // newborn — bright cyan flash
    if (age < 5)   return rgb565( 60, 210, 210);   // teal
    if (age < 15)  return rgb565( 70, 210,  80);   // green
    if (age < 40)  return rgb565(220, 200,  40);   // yellow
    if (age < 100) return rgb565(220, 110,  30);   // orange
    return              rgb565(160,  40,  20);     // elder — dim red
}

static void fill_cell(int gx, int gy, uint16_t color)
{
    int x0 = s_offx + gx * CELL_PX;
    int y0 = s_offy + gy * CELL_PX;
    for (int dy = 0; dy < CELL_PX - 1; dy++) {
        uint16_t *row = &s_buf[(y0 + dy) * s_w + x0];
        for (int dx = 0; dx < CELL_PX - 1; dx++) {
            row[dx] = color;
        }
    }
}

static void seed_random(void)
{
    int n = s_cols * s_rows;
    for (int i = 0; i < n; i++) {
        s_age[i] = ((esp_random() & 0xFF) < SEED_DENSITY) ? 1 : 0;
    }
    s_stale = 0;
    memset(s_hist, 0, sizeof(s_hist));
}

// FNV-1a over the alive bits — cheap enough at this grid size
static uint32_t board_hash(void)
{
    uint32_t h = 0x811c9dc5u;
    int n = s_cols * s_rows;
    for (int i = 0; i < n; i++) {
        h = (h ^ (s_age[i] ? 1u : 0u)) * 0x01000193u;
    }
    return h;
}

// ---------------------------------------------------------------------------
// generation step
// ---------------------------------------------------------------------------

static void step_generation(void)
{
    for (int y = 0; y < s_rows; y++) {
        int ym = (y == 0)          ? s_rows - 1 : y - 1;
        int yp = (y == s_rows - 1) ? 0          : y + 1;
        for (int x = 0; x < s_cols; x++) {
            int xm = (x == 0)          ? s_cols - 1 : x - 1;
            int xp = (x == s_cols - 1) ? 0          : x + 1;
            int n = (s_age[IDX(xm, ym)] != 0)
                  + (s_age[IDX(x,  ym)] != 0)
                  + (s_age[IDX(xp, ym)] != 0)
                  + (s_age[IDX(xm, y )] != 0)
                  + (s_age[IDX(xp, y )] != 0)
                  + (s_age[IDX(xm, yp)] != 0)
                  + (s_age[IDX(x,  yp)] != 0)
                  + (s_age[IDX(xp, yp)] != 0);
            int alive = (s_age[IDX(x, y)] != 0);
            s_next[IDX(x, y)] = (uint8_t)(alive ? (n == 2 || n == 3) : (n == 3));
        }
    }

    int pop = 0;
    int n = s_cols * s_rows;
    for (int i = 0; i < n; i++) {
        if (s_next[i]) {
            uint8_t a = s_age[i];
            s_age[i] = (a >= AGE_MAX) ? AGE_MAX : (uint8_t)(a + 1);
            pop++;
        } else {
            s_age[i] = 0;
        }
    }

    // Catches still-lifes (p1), blinkers (p2), and short oscillators up to p4.
    uint32_t h = board_hash();
    int repeating = (h == s_hist[0] || h == s_hist[1] || h == s_hist[2] || h == s_hist[3]);
    s_hist[3] = s_hist[2];
    s_hist[2] = s_hist[1];
    s_hist[1] = s_hist[0];
    s_hist[0] = h;

    if (pop == 0) {
        ESP_LOGI(TAG, "Extinct — reseeding");
        seed_random();
    } else if (repeating) {
        s_stale++;
        if (s_stale >= STALE_LIMIT) {
            ESP_LOGI(TAG, "Stagnant — reseeding");
            seed_random();
        }
    } else {
        s_stale = 0;
    }
}

static void render_board(void)
{
    memset(s_buf, 0, (size_t)s_w * (size_t)s_h * 2);
    for (int y = 0; y < s_rows; y++) {
        for (int x = 0; x < s_cols; x++) {
            uint8_t a = s_age[IDX(x, y)];
            if (a) fill_cell(x, y, color_for_age(a));
        }
    }
    lv_obj_invalidate(s_canvas);
}

// ---------------------------------------------------------------------------
// frame
// ---------------------------------------------------------------------------

static void frame_tick(lv_timer_t *t)
{
    (void)t;
    if (!s_buf || !s_canvas || !s_age || !s_next) return;
    step_generation();
    render_board();
}

// ---------------------------------------------------------------------------
// screen lifecycle
// ---------------------------------------------------------------------------

static void life_create(lv_obj_t *parent)
{
    s_w = DISPLAY_WIDTH;
    s_h = DISPLAY_HEIGHT;
    s_cols = s_w / CELL_PX;
    s_rows = s_h / CELL_PX;
    s_offx = (s_w - s_cols * CELL_PX) / 2;
    s_offy = (s_h - s_rows * CELL_PX) / 2;

    s_root = lv_obj_create(parent);
    lv_obj_remove_style_all(s_root);
    lv_obj_set_size(s_root, s_w, s_h);
    lv_obj_set_style_bg_color(s_root, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);

    size_t buf_sz = (size_t)s_w * (size_t)s_h * 2;
    size_t cells  = (size_t)s_cols * (size_t)s_rows;
    s_buf  = heap_caps_aligned_alloc(4, buf_sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_age  = heap_caps_malloc(cells, MALLOC_CAP_8BIT);
    s_next = heap_caps_malloc(cells, MALLOC_CAP_8BIT);
    if (!s_buf || !s_age || !s_next) {
        ESP_LOGE(TAG, "Alloc failed (canvas=%u, cells=%u)",
                 (unsigned)buf_sz, (unsigned)cells);
        return;
    }
    memset(s_buf,  0, buf_sz);
    memset(s_age,  0, cells);
    memset(s_next, 0, cells);

    s_canvas = lv_canvas_create(s_root);
    lv_canvas_set_buffer(s_canvas, s_buf, s_w, s_h, LV_COLOR_FORMAT_RGB565);
    lv_obj_align(s_canvas, LV_ALIGN_TOP_LEFT, 0, 0);

    seed_random();
    render_board();

    s_timer = lv_timer_create(frame_tick, FRAME_MS, NULL);

    ESP_LOGI(TAG, "Created (grid %dx%d cells, canvas %dx%d, %u B)",
             s_cols, s_rows, s_w, s_h, (unsigned)buf_sz);
}

static void life_destroy(void)
{
    if (s_timer)  { lv_timer_delete(s_timer);  s_timer  = NULL; }
    if (s_canvas) { lv_obj_delete(s_canvas);   s_canvas = NULL; }
    if (s_root)   { lv_obj_delete(s_root);     s_root   = NULL; }
    if (s_buf)    { heap_caps_free(s_buf);     s_buf    = NULL; }
    if (s_age)    { heap_caps_free(s_age);     s_age    = NULL; }
    if (s_next)   { heap_caps_free(s_next);    s_next   = NULL; }
    ESP_LOGI(TAG, "Destroyed");
}

const ui_screen_t screensaver_life = {
    .create      = life_create,
    .destroy     = life_destroy,
    .apply_theme = NULL,
    .on_event    = NULL,
    .on_input    = NULL,
    .name        = "ss_life",
};
