#pragma once
#include "lvgl.h"
#include <stdint.h>

// bg_opa: 0-100 % opacity of a theme-coloured plate behind the icon+text pair
// (readability over a wallpaper). 0 = no plate. The caller decides per screen.
void weather_widget_create(lv_obj_t *parent, int16_t x, int16_t y, int16_t w,
                           const lv_font_t *font, int16_t bg_opa);
void weather_widget_destroy(void);
void weather_widget_update(void);
void weather_widget_apply_theme(void);
