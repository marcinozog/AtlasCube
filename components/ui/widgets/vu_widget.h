#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Real-audio spectrum VU meter. A compact bar display in its own small container,
// fed by an FFT of the post-DSP PCM tapped in audio_levels. Contained, so only its
// own rectangle redraws rather than forcing the whole UI to recomposite. The FFT
// runs on its own task pinned to core 0 (off the LVGL/display core); the widget's
// LVGL timer is render-only. The audio hot path stays a cheap ring append.
//
// transparent=false fills the container with the theme's vu_bg (cheapest: a changed
// strip is a solid fill). transparent=true draws no background, so the bars sit
// directly on the screen behind (wallpaper/gradient); each changed strip then also
// re-blits that background, still delta-bounded — fine for a small VU.
//
// One instance at a time (radio or SD-player screen — only one is shown at once).
// create() spins up the refresh timer; destroy() tears it and the bars down.
void vu_widget_create(lv_obj_t *parent, int x, int y, int w, int h, bool transparent);
void vu_widget_destroy(void);

// Recolour bars + container from the active theme. Safe to call when not created.
void vu_widget_apply_theme(void);

#ifdef __cplusplus
}
#endif
