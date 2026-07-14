#pragma once

#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ANIMATED_WHEELS_CASSETTE = 0,
    ANIMATED_WHEELS_CAR_RIMS = 1,
} animated_wheels_style_t;

// Two lightweight animated circular overlays positioned in absolute LCD
// coordinates. X/Y are the top-left corner of each square; size is its width
// and height. The selected style can be placed over a cassette or car image.
void animated_wheels_widget_create(lv_obj_t *parent, animated_wheels_style_t style,
                                   int16_t left_x,  int16_t left_y,  int16_t left_size,
                                   int16_t right_x, int16_t right_y, int16_t right_size);
void animated_wheels_widget_destroy(void);
void animated_wheels_widget_set_running(bool running);
void animated_wheels_widget_apply_theme(void);

#ifdef __cplusplus
}
#endif
