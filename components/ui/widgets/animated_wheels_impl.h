#pragma once

#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Binary boundary shared with lib/libanimated_wheels_impl.a. Keep this ABI in
// sync with TMP/private/animated_wheels/components/animated_wheels_impl.
typedef enum {
    ANIMATED_WHEELS_IMPL_CASSETTE = 0,
    ANIMATED_WHEELS_IMPL_CAR_RIMS = 1,
} animated_wheels_impl_style_t;

typedef struct {
    uint32_t accent;
    uint32_t text_muted;
    uint32_t text_secondary;
    uint32_t status_ok;
} animated_wheels_impl_palette_t;

void animated_wheels_impl_create(lv_obj_t *parent, animated_wheels_impl_style_t style,
                                 int16_t left_x,  int16_t left_y,  int16_t left_size,
                                 int16_t right_x, int16_t right_y, int16_t right_size,
                                 int16_t display_width, int16_t display_height,
                                 const animated_wheels_impl_palette_t *palette);
void animated_wheels_impl_destroy(void);
void animated_wheels_impl_set_running(bool running);
void animated_wheels_impl_apply_palette(const animated_wheels_impl_palette_t *palette);

#ifdef __cplusplus
}
#endif
