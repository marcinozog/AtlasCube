#include "screensaver_fireworks.h"
#include "ui_screen.h"
#include "ui_profile.h"
#include "lvgl.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_random.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

static const char *TAG = "SS_FIREWORKS";

#define ROCKET_COUNT       6
#define SPARKS_PER_ROCKET  36
#define GRAVITY            0.10f
#define DRAG               0.985f
#define SPARK_LIFE_FRAMES  70
#define FRAME_MS           33      // ~30 fps

typedef struct {
    float x, y;
    float vx, vy;
    int   life;        // remaining frames; 0 = dead
} spark_t;

typedef enum {
    ROCKET_IDLE = 0,
    ROCKET_RISING,
    ROCKET_EXPLODED,
} rocket_state_t;

typedef struct {
    rocket_state_t state;
    float          x,  y;
    float          vx, vy;
    float          px, py;          // previous position, for the rising trail
    uint8_t        r,  g,  b;
    int            cooldown;        // frames before next launch attempt
    spark_t        sparks[SPARKS_PER_ROCKET];
} rocket_t;

static lv_obj_t   *s_root   = NULL;
static lv_obj_t   *s_canvas = NULL;
static uint16_t   *s_buf    = NULL;
static lv_timer_t *s_timer  = NULL;
static rocket_t    s_rockets[ROCKET_COUNT];
static int         s_w = 0, s_h = 0;

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

static inline float frand_pos(void)             // [0, 1)
{
    return (float)(esp_random() & 0xFFFF) / 65536.0f;
}

static inline float frand_signed(void)          // [-1, 1)
{
    return frand_pos() * 2.0f - 1.0f;
}

static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

static inline void put_px(int x, int y, uint16_t c)
{
    if ((unsigned)x < (unsigned)s_w && (unsigned)y < (unsigned)s_h) {
        s_buf[y * s_w + x] = c;
    }
}

// 2×2 block — sparks look like a "speck" instead of a single pixel
// on small high-DPI panels.
static inline void put_px_2x2(int x, int y, uint16_t c)
{
    put_px(x,     y,     c);
    put_px(x + 1, y,     c);
    put_px(x,     y + 1, c);
    put_px(x + 1, y + 1, c);
}

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
// rocket logic
// ---------------------------------------------------------------------------

static void launch_rocket(rocket_t *r)
{
    static const uint8_t palette[7][3] = {
        { 255,  90,  70 },   // red
        { 255, 200,  60 },   // gold
        { 130, 220, 255 },   // cyan
        { 200, 130, 255 },   // purple
        { 255, 130, 200 },   // pink
        { 130, 255, 150 },   // green
        { 255, 255, 255 },   // white
    };
    uint32_t p = esp_random() % 7;

    r->state = ROCKET_RISING;
    r->x     = frand_pos() * (s_w - 60) + 30;
    r->y     = s_h - 4;
    r->px    = r->x;
    r->py    = r->y;
    r->vx    = frand_signed() * 0.6f;
    r->vy    = -5.2f - frand_pos() * 1.6f;     // upward; apex ≈ vy²/(2·GRAVITY)
    r->r     = palette[p][0];
    r->g     = palette[p][1];
    r->b     = palette[p][2];
}

static void explode(rocket_t *r)
{
    r->state = ROCKET_EXPLODED;
    for (int i = 0; i < SPARKS_PER_ROCKET; i++) {
        float ang = frand_pos() * 6.2832f;
        float spd = 0.8f + frand_pos() * 1.6f;
        r->sparks[i].x  = r->x;
        r->sparks[i].y  = r->y;
        r->sparks[i].vx = cosf(ang) * spd + r->vx * 0.3f;
        r->sparks[i].vy = sinf(ang) * spd + r->vy * 0.3f;
        r->sparks[i].life = SPARK_LIFE_FRAMES - (int)(frand_pos() * 20.0f);
    }
}

static void update_rocket(rocket_t *r)
{
    if (r->state == ROCKET_IDLE) {
        if (r->cooldown > 0) { r->cooldown--; return; }
        // small probability per frame to launch — staggers rockets
        if ((esp_random() & 0x3F) < 4) launch_rocket(r);
        return;
    }

    if (r->state == ROCKET_RISING) {
        r->px = r->x;
        r->py = r->y;
        r->x  += r->vx;
        r->y  += r->vy;
        r->vy += GRAVITY;

        // bright rising trail
        uint16_t col = rgb565(r->r, r->g, r->b);
        draw_line((int)r->px, (int)r->py, (int)r->x, (int)r->y, col);

        // explode at apex (or before going off the top)
        if (r->vy >= 0.0f || r->y < s_h * 0.20f) {
            explode(r);
        }
        return;
    }

    // EXPLODED — update sparks
    int alive = 0;
    for (int i = 0; i < SPARKS_PER_ROCKET; i++) {
        spark_t *s = &r->sparks[i];
        if (s->life <= 0) continue;
        alive++;

        s->x  += s->vx;
        s->y  += s->vy;
        s->vx *= DRAG;
        s->vy = s->vy * DRAG + GRAVITY;
        s->life--;

        if (s->x < 0 || s->x >= s_w || s->y < 0 || s->y >= s_h) {
            s->life = 0;
            continue;
        }

        // brightness decays with remaining life
        float bf = (float)s->life / (float)SPARK_LIFE_FRAMES;
        uint8_t rr = (uint8_t)(r->r * bf);
        uint8_t gg = (uint8_t)(r->g * bf);
        uint8_t bb = (uint8_t)(r->b * bf);
        put_px_2x2((int)s->x, (int)s->y, rgb565(rr, gg, bb));
    }

    if (alive == 0) {
        r->state    = ROCKET_IDLE;
        r->cooldown = 10 + (int)(frand_pos() * 30.0f);
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

    for (int i = 0; i < ROCKET_COUNT; i++) {
        update_rocket(&s_rockets[i]);
    }

    lv_obj_invalidate(s_canvas);
}

// ---------------------------------------------------------------------------
// screen lifecycle
// ---------------------------------------------------------------------------

static void fireworks_create(lv_obj_t *parent)
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

    for (int i = 0; i < ROCKET_COUNT; i++) {
        s_rockets[i].state    = ROCKET_IDLE;
        s_rockets[i].cooldown = (int)(frand_pos() * 60.0f);
    }

    s_timer = lv_timer_create(frame_tick, FRAME_MS, NULL);

    ESP_LOGI(TAG, "Created (canvas %dx%d in PSRAM = %u B)", s_w, s_h, (unsigned)sz);
}

static void fireworks_destroy(void)
{
    if (s_timer)  { lv_timer_delete(s_timer);  s_timer  = NULL; }
    if (s_canvas) { lv_obj_delete(s_canvas);   s_canvas = NULL; }
    if (s_root)   { lv_obj_delete(s_root);     s_root   = NULL; }
    if (s_buf)    { heap_caps_free(s_buf);     s_buf    = NULL; }
    ESP_LOGI(TAG, "Destroyed");
}

const ui_screen_t screensaver_fireworks = {
    .create      = fireworks_create,
    .destroy     = fireworks_destroy,
    .apply_theme = NULL,
    .on_event    = NULL,
    .on_input    = NULL,
    .name        = "ss_fireworks",
};
