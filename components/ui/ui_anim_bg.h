#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Decorative animated VU-meter background. A row of rounded, gradient-coloured
// bars (orange → magenta) painted on lv_layer_bottom() — the layer that always
// sits behind the active screen and survives lv_obj_clean() between navigations.
// To make it visible the screen background must be transparent; ui_background.c
// handles that when the "vu" wallpaper sentinel is active.
//
// The motion is purely cosmetic (smoothed random walk), not driven by real audio.
// On mono panels every function is a no-op.

// Create the backdrop + bars on the bottom layer and start the animation timer.
// Idempotent: a second call while already running does nothing.
void ui_anim_bg_start(void);

// Delete the bars/backdrop and stop the timer. Safe to call when not running.
void ui_anim_bg_stop(void);

// Recolour the backdrop from the active theme (bg_primary). Safe to call when
// not running. Lets a live dark/light switch update the VU background without
// restarting the animation.
void ui_anim_bg_apply_theme(void);

// True while the animation is live.
bool ui_anim_bg_active(void);

#ifdef __cplusplus
}
#endif
