#pragma once
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Touch overlay shown over screen_radio / screen_bt: a cross of 5 round
// buttons — play/pause in the center, vol+/vol- top/bottom, prev/next on the
// sides. Tap on the screen background to show it; tap on the dimmed area
// outside the buttons or wait for the auto-hide timeout to dismiss.
//
// Buttons are visual placeholders for now — no action handlers wired.
void controls_overlay_create(lv_obj_t *parent);
void controls_overlay_destroy(void);

#ifdef __cplusplus
}
#endif
