#pragma once
#include "lvgl.h"
#include <stdint.h>

// A background plate behind the icon+text pair (readability over a wallpaper) is
// applied automatically per the global display.label_bg / label_bg_opa setting.
void weather_widget_create(lv_obj_t *parent, int16_t x, int16_t y, int16_t w,
                           const lv_font_t *font);
void weather_widget_destroy(void);
void weather_widget_update(void);
void weather_widget_apply_theme(void);
