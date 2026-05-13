#include "screensaver_clockhands.h"
#include "ui_screen.h"
#include "ui_profile.h"
#include "lvgl.h"
#include "esp_log.h"
#include <math.h>
#include <time.h>

static const char *TAG = "SS_CLOCKHANDS";

#define TICK_MS  1000

static lv_obj_t   *s_root   = NULL;
static lv_obj_t   *s_hand_h = NULL;
static lv_obj_t   *s_hand_m = NULL;
static lv_obj_t   *s_hand_s = NULL;
static lv_timer_t *s_timer  = NULL;

static lv_point_precise_t s_pt_h[2];
static lv_point_precise_t s_pt_m[2];
static lv_point_precise_t s_pt_s[2];
static lv_point_precise_t s_pt_tick[12][2];

static int s_cx, s_cy;
static int s_r_hour, s_r_min, s_r_sec;

static void set_hand(lv_obj_t *line, lv_point_precise_t pts[2], float angle, int r)
{
    pts[0].x = s_cx;
    pts[0].y = s_cy;
    pts[1].x = s_cx + (lv_value_precise_t)(sinf(angle) * (float)r);
    pts[1].y = s_cy - (lv_value_precise_t)(cosf(angle) * (float)r);
    lv_line_set_points_mutable(line, pts, 2);
}

static void update_hands(void)
{
    if (!s_hand_h) return;

    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);

    float sec = (float)t.tm_sec;
    float min = (float)t.tm_min + sec / 60.0f;
    float hr  = (float)(t.tm_hour % 12) + min / 60.0f;

    const float two_pi = 2.0f * 3.14159265f;
    set_hand(s_hand_s, s_pt_s, sec * (two_pi / 60.0f), s_r_sec);
    set_hand(s_hand_m, s_pt_m, min * (two_pi / 60.0f), s_r_min);
    set_hand(s_hand_h, s_pt_h, hr  * (two_pi / 12.0f), s_r_hour);
}

static void tick_cb(lv_timer_t *t) { (void)t; update_hands(); }

static lv_obj_t *make_hand(lv_obj_t *parent, lv_point_precise_t pts[2],
                            int width, uint32_t color)
{
    pts[0].x = s_cx; pts[0].y = s_cy;
    pts[1].x = s_cx; pts[1].y = s_cy;

    lv_obj_t *l = lv_line_create(parent);
    lv_line_set_points_mutable(l, pts, 2);
    lv_obj_set_style_line_width(l, width, LV_PART_MAIN);
    lv_obj_set_style_line_color(l, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_line_rounded(l, true, LV_PART_MAIN);
    return l;
}

static void draw_face(lv_obj_t *parent, int radius)
{
    for (int i = 0; i < 12; i++) {
        float a = (float)i * (2.0f * 3.14159265f / 12.0f);
        int r_inner = radius - ((i % 3 == 0) ? 16 : 8);
        int r_outer = radius - 2;

        s_pt_tick[i][0].x = s_cx + (lv_value_precise_t)(sinf(a) * (float)r_inner);
        s_pt_tick[i][0].y = s_cy - (lv_value_precise_t)(cosf(a) * (float)r_inner);
        s_pt_tick[i][1].x = s_cx + (lv_value_precise_t)(sinf(a) * (float)r_outer);
        s_pt_tick[i][1].y = s_cy - (lv_value_precise_t)(cosf(a) * (float)r_outer);

        lv_obj_t *tick = lv_line_create(parent);
        lv_line_set_points(tick, s_pt_tick[i], 2);
        lv_obj_set_style_line_width(tick, (i % 3 == 0) ? 4 : 2, LV_PART_MAIN);
        lv_obj_set_style_line_color(tick,
            (i % 3 == 0) ? lv_color_white() : lv_color_hex(0x808080),
            LV_PART_MAIN);
        lv_obj_set_style_line_rounded(tick, false, LV_PART_MAIN);
    }
}

static void clockhands_create(lv_obj_t *parent)
{
    const int W = DISPLAY_WIDTH;
    const int H = DISPLAY_HEIGHT;
    s_cx = W / 2;
    s_cy = H / 2;

    const int radius = (W < H ? W : H) / 2 - 6;

    s_r_sec  = radius - 18;
    s_r_min  = (int)(radius * 0.80f);
    s_r_hour = (int)(radius * 0.55f);

    s_root = lv_obj_create(parent);
    lv_obj_remove_style_all(s_root);
    lv_obj_set_size(s_root, W, H);
    lv_obj_set_style_bg_color(s_root, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);

    draw_face(s_root, radius);

    s_hand_h = make_hand(s_root, s_pt_h, 8, 0xFFFFFF);
    s_hand_m = make_hand(s_root, s_pt_m, 5, 0xFFFFFF);
    s_hand_s = make_hand(s_root, s_pt_s, 2, 0xE03030);

    lv_obj_t *hub = lv_obj_create(s_root);
    lv_obj_remove_style_all(hub);
    lv_obj_set_size(hub, 12, 12);
    lv_obj_set_pos(hub, s_cx - 6, s_cy - 6);
    lv_obj_set_style_radius(hub, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(hub, lv_color_hex(0xE03030), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(hub, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(hub, LV_OBJ_FLAG_SCROLLABLE);

    update_hands();
    s_timer = lv_timer_create(tick_cb, TICK_MS, NULL);

    ESP_LOGI(TAG, "Created (%dx%d, R=%d)", W, H, radius);
}

static void clockhands_destroy(void)
{
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    if (s_root)  { lv_obj_delete(s_root);    s_root  = NULL; }
    s_hand_h = s_hand_m = s_hand_s = NULL;
    ESP_LOGI(TAG, "Destroyed");
}

const ui_screen_t screensaver_clockhands = {
    .create      = clockhands_create,
    .destroy     = clockhands_destroy,
    .apply_theme = NULL,
    .on_event    = NULL,
    .on_input    = NULL,
    .name        = "ss_clockhands",
};
