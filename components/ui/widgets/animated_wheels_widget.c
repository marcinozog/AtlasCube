#include "animated_wheels_widget.h"

#include "animated_wheels_impl.h"
#include "theme.h"
#include "ui_profile.h"

// Public AtlasCube adapter. Drawing, geometry and animation state live in the
// private prebuilt library; this file only translates the current UI theme and
// compile-time panel dimensions across the stable binary boundary.
static animated_wheels_impl_palette_t current_palette(void)
{
    const ui_theme_colors_t *theme = theme_get();
    return (animated_wheels_impl_palette_t) {
        .accent         = theme->accent,
        .text_muted     = theme->text_muted,
        .text_secondary = theme->text_secondary,
        .status_ok      = theme->status_ok,
    };
}

void animated_wheels_widget_create(lv_obj_t *parent, animated_wheels_style_t style,
                                   bool show_left,
                                   int16_t left_x, int16_t left_y, int16_t left_size,
                                   bool show_right,
                                   int16_t right_x, int16_t right_y, int16_t right_size)
{
    animated_wheels_impl_palette_t palette = current_palette();
    // The private renderer has a stable two-wheel ABI. Place a disabled wheel
    // just beyond the display clip instead of changing that binary boundary.
    if (!show_left)  { left_x  = DISPLAY_WIDTH; left_y  = DISPLAY_HEIGHT; }
    if (!show_right) { right_x = DISPLAY_WIDTH; right_y = DISPLAY_HEIGHT; }
    animated_wheels_impl_create(parent, (animated_wheels_impl_style_t)style,
                                left_x, left_y, left_size,
                                right_x, right_y, right_size,
                                DISPLAY_WIDTH, DISPLAY_HEIGHT, &palette);
}

void animated_wheels_widget_destroy(void)
{
    animated_wheels_impl_destroy();
}

void animated_wheels_widget_set_running(bool running)
{
    animated_wheels_impl_set_running(running);
}

void animated_wheels_widget_apply_theme(void)
{
    animated_wheels_impl_palette_t palette = current_palette();
    animated_wheels_impl_apply_palette(&palette);
}
