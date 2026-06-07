#include "screensaver_clockdigital.h"
#include "ui_screen.h"
#include "ui_profile.h"
#include "theme.h"
#include "fonts/ui_fonts.h"
#include "ntp_service.h"
#include "lvgl.h"
#include "esp_log.h"
#include <time.h>
#include <stdio.h>

static const char *TAG = "SS_CLOCKDIGI";

#define TICK_MS  1000

static lv_obj_t   *s_root    = NULL;
static lv_obj_t   *s_hh      = NULL;
static lv_obj_t   *s_colon   = NULL;
static lv_obj_t   *s_mm      = NULL;
static lv_obj_t   *s_date    = NULL;   // NULL on short panels (no room)
static lv_timer_t *s_timer   = NULL;
static bool        s_blink   = true;   // colon visible this tick?

// Big time font. The custom _72/_96/_120 fonts are digit-only (0x30-0x3A =
// digits + colon), which is exactly what HH:MM needs. Short/mono panels fall
// back to the built-in montserrat_48 (full glyphs, fits a 64px-tall display).
static const lv_font_t *time_font(void)
{
    int h = DISPLAY_HEIGHT;
    if (h >= 460) return &lv_font_montserrat_120;
    if (h >= 280) return &lv_font_montserrat_96;
    if (h >= 150) return &lv_font_montserrat_72;
    return &lv_font_montserrat_48;
}

// Date line uses _pl fonts (full glyphs). Only shown when the panel is tall
// enough for a second line under the clock.
static const lv_font_t *date_font(void)
{
    int h = DISPLAY_HEIGHT;
    if (h >= 460) return &lv_font_montserrat_24_pl;
    if (h >= 280) return &lv_font_montserrat_18_pl;
    return &lv_font_montserrat_14_pl;
}

static void update_clock(void)
{
    if (!s_hh) return;

    char hh[4] = "00";
    char mm[4] = "00";
    bool synced = ntp_service_is_synced();

    struct tm t = {0};
    if (synced) {
        time_t now = time(NULL);
        localtime_r(&now, &t);
        snprintf(hh, sizeof(hh), "%02d", t.tm_hour);
        snprintf(mm, sizeof(mm), "%02d", t.tm_min);
    }

    lv_label_set_text(s_hh, hh);
    lv_label_set_text(s_mm, mm);

    // Blink the colon every tick; keep it steady while waiting for NTP so a
    // flashing "00:00" doesn't look like a working clock.
    s_blink = synced ? !s_blink : true;
    lv_obj_set_style_opa(s_colon, s_blink ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_MAIN);

    if (s_date) {
        if (synced) {
            char buf[24];
            strftime(buf, sizeof(buf), "%a %d.%m.%Y", &t);
            lv_label_set_text(s_date, buf);
        } else {
            lv_label_set_text(s_date, "--");
        }
    }
}

static void tick_cb(lv_timer_t *t) { (void)t; update_clock(); }

static lv_obj_t *make_digit(lv_obj_t *parent, const lv_font_t *font, uint32_t color)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, lv_color_hex(color), LV_PART_MAIN);
    return l;
}

static void clockdigital_create(lv_obj_t *parent)
{
    const int W = DISPLAY_WIDTH;
    const int H = DISPLAY_HEIGHT;
    const bool show_date = (H >= 150);

    const ui_theme_colors_t *th = theme_get();
    const lv_font_t *tf = time_font();

    s_root = lv_obj_create(parent);
    lv_obj_remove_style_all(s_root);
    lv_obj_set_size(s_root, W, H);
    lv_obj_set_style_bg_color(s_root, lv_color_hex(th->bg_primary), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(s_root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_root, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // HH : MM as three labels so the colon can blink independently (the
    // digit-only big fonts have no space glyph to swap in for a one-label blink).
    lv_obj_t *row = lv_obj_create(s_root);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    s_hh    = make_digit(row, tf, th->text_primary);
    s_colon = make_digit(row, tf, th->text_primary);
    s_mm    = make_digit(row, tf, th->text_primary);
    lv_label_set_text(s_colon, ":");

    if (show_date) {
        s_date = lv_label_create(s_root);
        lv_obj_set_style_text_font(s_date, date_font(), LV_PART_MAIN);
        lv_obj_set_style_text_color(s_date, lv_color_hex(th->text_muted), LV_PART_MAIN);
        lv_obj_set_style_pad_top(s_date, H / 32, LV_PART_MAIN);
    }

    update_clock();
    s_timer = lv_timer_create(tick_cb, TICK_MS, NULL);

    ESP_LOGI(TAG, "Created (%dx%d, date=%d)", W, H, show_date);
}

static void clockdigital_destroy(void)
{
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    if (s_root)  { lv_obj_delete(s_root);    s_root  = NULL; }
    s_hh = s_colon = s_mm = s_date = NULL;
    ESP_LOGI(TAG, "Destroyed");
}

const ui_screen_t screensaver_clockdigital = {
    .create      = clockdigital_create,
    .destroy     = clockdigital_destroy,
    .apply_theme = NULL,
    .on_event    = NULL,
    .on_input    = NULL,
    .name        = "ss_clockdigital",
};
