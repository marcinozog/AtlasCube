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
// The owning screen passes its mode so the widget knows which audio source
// the buttons control. Don't infer it from app_state.screen — that holds the
// user's persisted preference, not the currently displayed screen, and the
// two can legitimately diverge (e.g. alarm pops up RADIO while BT was saved).
typedef enum {
    CTRL_OVL_MODE_RADIO,
    CTRL_OVL_MODE_BT,
    CTRL_OVL_MODE_SD,
} controls_overlay_mode_t;

void controls_overlay_create(lv_obj_t *parent, controls_overlay_mode_t mode);
void controls_overlay_destroy(void);

#ifdef __cplusplus
}
#endif
