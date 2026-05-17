#pragma once
#include "lvgl.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Volume overlay shown briefly on top of the screen after a volume change.
// Single-instance: a second call before the auto-hide timeout reuses the
// existing overlay and resets the timer.
//
// parent: LVGL object the overlay is created on (usually the screen root).
// vol:    0..100, formatted as "NN%".
// large:  pick the big visual variant (large font + bigger box). Intended for
//         screens with a huge clock font; pass false elsewhere.
void vol_overlay_show(lv_obj_t *parent, int vol, bool large);

// Hide and free the overlay + its timer. Screens must call this in their
// destroy() so the timer doesn't outlive the LVGL objects it would touch.
void vol_overlay_hide(void);

#ifdef __cplusplus
}
#endif
