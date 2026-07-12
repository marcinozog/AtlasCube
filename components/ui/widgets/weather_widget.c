#include "weather_widget.h"
#include "weather.h"
#include "theme.h"
#include "fonts/ui_fonts.h"
#include "ui_profile.h"
#include <math.h>
#include <stdio.h>

static lv_obj_t *s_label;
static lv_timer_t *s_timer;

static const char *condition(int code)
{
    if (code == 0) return "Clear";
    if (code <= 2) return "Partly cloudy";
    if (code == 3) return "Cloudy";
    if (code == 45 || code == 48) return "Fog";
    if (code >= 51 && code <= 67) return "Rain";
    if (code >= 71 && code <= 77) return "Snow";
    if (code >= 80 && code <= 82) return "Showers";
    if (code >= 85 && code <= 86) return "Snow showers";
    if (code >= 95) return "Storm";
    return "Weather";
}

static void tick(lv_timer_t *t) { (void)t; weather_widget_update(); }

void weather_widget_create(lv_obj_t *parent, int16_t x, int16_t y, int16_t w,
                           const lv_font_t *font)
{
    s_label = lv_label_create(parent);
    lv_label_set_long_mode(s_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(s_label, w > 0 ? w : DISPLAY_WIDTH);
    lv_obj_set_pos(s_label, x, y);
    lv_obj_set_style_text_align(s_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_label, font ? font : &lv_font_montserrat_12_pl, LV_PART_MAIN);
    weather_widget_update();
    s_timer = lv_timer_create(tick, 15000, NULL);
}

void weather_widget_destroy(void)
{
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    s_label = NULL;
}

void weather_widget_update(void)
{
    if (!s_label) return;
    weather_data_t data;
    weather_get(&data);
    if (!data.valid) { lv_obj_add_flag(s_label, LV_OBJ_FLAG_HIDDEN); return; }
    lv_obj_clear_flag(s_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_color(s_label, lv_color_hex(theme_get()->text_secondary), LV_PART_MAIN);
    // LVGL's built-in sprintf has no %f support (LV_SPRINTF_USE_FLOAT is off) —
    // a float here renders garbage, so round to a whole degree first.
    lv_label_set_text_fmt(s_label, "%+d C  %s  %d%%",
                          (int)lroundf(data.temperature_c),
                          condition(data.weather_code), data.humidity_pct);
}

void weather_widget_apply_theme(void) { weather_widget_update(); }
