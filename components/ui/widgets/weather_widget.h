#pragma once
#include "lvgl.h"
#include <stdint.h>

// A background plate behind the icon+text pair uses the owning screen's profile.
void weather_widget_create(lv_obj_t *parent, int16_t x, int16_t y, int16_t w,
                           const lv_font_t *font, int label_bg_opa);
void weather_widget_destroy(void);
void weather_widget_update(void);
void weather_widget_apply_theme(void);
