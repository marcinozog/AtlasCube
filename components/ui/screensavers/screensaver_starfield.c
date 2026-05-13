#include "screensaver_starfield.h"
#include "ui_screen.h"
#include "ui_profile.h"
#include "lvgl.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_random.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "SS_STARFIELD";

#define STAR_COUNT 150
#define STAR_SPEED 0.018f       // delta-z per frame
#define STAR_FOV   140.0f       // perspective scale (px); higher → faster spread
#define STAR_Z_MIN 0.05f
#define FRAME_MS   33           // ~30 fps

typedef struct {
    float x, y, z;   // world coords; x,y in [-1,1], z in (0,1]
    float pz;        // previous z, for motion-streak length
} star_t;

static lv_obj_t   *s_root   = NULL;
static lv_obj_t   *s_canvas = NULL;
static uint16_t   *s_buf    = NULL;
static lv_timer_t *s_timer  = NULL;
static star_t      s_stars[STAR_COUNT];
static int         s_w = 0, s_h = 0;

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

static inline float frand_unit(void)    // [-1, 1)
{
    return ((float)(esp_random() & 0xFFFF) / 32768.0f) - 1.0f;
}

static inline float frand_pos(void)     // (0, 1]
{
    uint32_t r = esp_random() & 0xFFFF;
    if (r == 0) r = 1;
    return (float)r / 65535.0f;
}

static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

static void star_respawn(star_t *s)
{
    s->x  = frand_unit();
    s->y  = frand_unit();
    s->z  = frand_pos();
    if (s->z < STAR_Z_MIN) s->z = STAR_Z_MIN;
    s->pz = s->z;
}

static inline void put_px(int x, int y, uint16_t c)
{
    if ((unsigned)x < (unsigned)s_w && (unsigned)y < (unsigned)s_h) {
        s_buf[y * s_w + x] = c;
    }
}

// Bresenham — short streaks, no AA needed for 30 fps starfield
static void draw_line(int x0, int y0, int x1, int y1, uint16_t c)
{
    int dx =  abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (1) {
        put_px(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

// ---------------------------------------------------------------------------
// frame
// ---------------------------------------------------------------------------

static void frame_tick(lv_timer_t *t)
{
    (void)t;
    if (!s_buf || !s_canvas) return;

    memset(s_buf, 0, (size_t)s_w * (size_t)s_h * 2);

    const float cx = s_w * 0.5f;
    const float cy = s_h * 0.5f;

    for (int i = 0; i < STAR_COUNT; i++) {
        star_t *st = &s_stars[i];

        st->pz = st->z;
        st->z -= STAR_SPEED;
        if (st->z <= STAR_Z_MIN) {
            star_respawn(st);
            continue;
        }

        float k_now  = STAR_FOV / st->z;
        float k_prev = STAR_FOV / st->pz;

        int x_now  = (int)(st->x * k_now  + cx);
        int y_now  = (int)(st->y * k_now  + cy);
        int x_prev = (int)(st->x * k_prev + cx);
        int y_prev = (int)(st->y * k_prev + cy);

        // off-screen → respawn so density stays roughly constant
        if (x_now < -50 || x_now > s_w + 50 ||
            y_now < -50 || y_now > s_h + 50) {
            star_respawn(st);
            continue;
        }

        // brightness: closer → brighter; quadratic ramp pops nicer than linear
        float bf = 1.0f - st->z;
        if (bf < 0.0f) bf = 0.0f;
        bf = bf * bf;
        uint8_t v = (uint8_t)(bf * 255.0f);
        uint16_t col = rgb565(v, v, v);

        draw_line(x_prev, y_prev, x_now, y_now, col);

        // beefy core for close stars
        if (st->z < 0.3f) {
            put_px(x_now + 1, y_now,     col);
            put_px(x_now,     y_now + 1, col);
        }
    }

    lv_obj_invalidate(s_canvas);
}

// ---------------------------------------------------------------------------
// screen lifecycle
// ---------------------------------------------------------------------------

static void starfield_create(lv_obj_t *parent)
{
    s_w = DISPLAY_WIDTH;
    s_h = DISPLAY_HEIGHT;

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

    for (int i = 0; i < STAR_COUNT; i++) {
        star_respawn(&s_stars[i]);
        // re-randomize z so initial frame isn't a uniform shell
        s_stars[i].z  = frand_pos();
        if (s_stars[i].z < STAR_Z_MIN) s_stars[i].z = STAR_Z_MIN;
        s_stars[i].pz = s_stars[i].z;
    }

    s_timer = lv_timer_create(frame_tick, FRAME_MS, NULL);

    ESP_LOGI(TAG, "Created (canvas %dx%d in PSRAM = %u B)", s_w, s_h, (unsigned)sz);
}

static void starfield_destroy(void)
{
    if (s_timer)  { lv_timer_delete(s_timer);  s_timer  = NULL; }
    if (s_canvas) { lv_obj_delete(s_canvas);   s_canvas = NULL; }
    if (s_root)   { lv_obj_delete(s_root);     s_root   = NULL; }
    if (s_buf)    { heap_caps_free(s_buf);     s_buf    = NULL; }
    ESP_LOGI(TAG, "Destroyed");
}

const ui_screen_t screensaver_starfield = {
    .create      = starfield_create,
    .destroy     = starfield_destroy,
    .apply_theme = NULL,
    .on_event    = NULL,
    .on_input    = NULL,
    .name        = "ss_starfield",
};
