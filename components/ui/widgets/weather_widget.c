#include "weather_widget.h"
#include "weather.h"
#include "theme.h"
#include "fonts/ui_fonts.h"
#include "ui_profile.h"
#include <math.h>
#include <stdio.h>

// Icon + text, centered as a pair inside a w-wide flex row at (x,y) — the
// text length varies per condition, so the pair can't be centered with two
// absolutely-positioned labels.
static lv_obj_t *s_row, *s_icon, *s_label;
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

// Weather Icons glyphs (UTF-8) — only the codepoints baked into
// lv_font_weather_20.c (see its Opts header line).
static const char *icon_glyph(int code, bool day)
{
    if (code == 0)  return day ? "\xEF\x80\x8D" : "\xEF\x80\xAE";  // day-sunny / night-clear
    if (code <= 2)  return day ? "\xEF\x80\x82" : "\xEF\x82\x86";  // day-cloudy / night-alt-cloudy
    if (code == 3)  return "\xEF\x80\x93";                          // cloudy
    if (code == 45 || code == 48)  return "\xEF\x80\x94";           // fog
    if (code >= 51 && code <= 67)  return "\xEF\x80\x99";           // rain
    if (code >= 71 && code <= 77)  return "\xEF\x80\x9B";           // snow
    if (code >= 80 && code <= 82)  return "\xEF\x80\x9A";           // showers
    if (code >= 85 && code <= 86)  return "\xEF\x80\x9B";           // snow showers
    if (code >= 95) return "\xEF\x80\x9E";                          // thunderstorm
    return "\xEF\x80\x93";
}

static void tick(lv_timer_t *t) { (void)t; weather_widget_update(); }

void weather_widget_create(lv_obj_t *parent, int16_t x, int16_t y, int16_t w,
                           const lv_font_t *font)
{
    s_row = lv_obj_create(parent);
    lv_obj_remove_style_all(s_row);
    lv_obj_set_size(s_row, w > 0 ? w : DISPLAY_WIDTH, LV_SIZE_CONTENT);
    lv_obj_set_pos(s_row, x, y);
    lv_obj_set_flex_flow(s_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(s_row, 6, LV_PART_MAIN);
    lv_obj_clear_flag(s_row, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    s_icon = lv_label_create(s_row);
    lv_obj_set_style_text_font(s_icon, &lv_font_weather_20, LV_PART_MAIN);

    s_label = lv_label_create(s_row);
    lv_obj_set_style_text_font(s_label, font ? font : &lv_font_montserrat_12_pl,
                               LV_PART_MAIN);

    weather_widget_update();
    s_timer = lv_timer_create(tick, 15000, NULL);
}

void weather_widget_destroy(void)
{
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    s_row = s_icon = s_label = NULL;
}

void weather_widget_update(void)
{
    if (!s_row) return;
    weather_data_t data;
    weather_get(&data);
    if (!data.valid) { lv_obj_add_flag(s_row, LV_OBJ_FLAG_HIDDEN); return; }
    lv_obj_clear_flag(s_row, LV_OBJ_FLAG_HIDDEN);
    lv_color_t col = lv_color_hex(theme_get()->text_secondary);
    lv_obj_set_style_text_color(s_icon, col, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_label, col, LV_PART_MAIN);
    lv_label_set_text(s_icon, icon_glyph(data.weather_code, data.is_day));
    // LVGL's built-in sprintf has no %f support (LV_SPRINTF_USE_FLOAT is off) —
    // a float here renders garbage, so round to a whole degree first.
    lv_label_set_text_fmt(s_label, "%+d C  %s  %d%%",
                          (int)lroundf(data.temperature_c),
                          condition(data.weather_code), data.humidity_pct);
}

void weather_widget_apply_theme(void) { weather_widget_update(); }
