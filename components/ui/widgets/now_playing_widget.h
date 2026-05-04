#pragma once
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * "Now playing" widget: station name (always) + ICY title below (when available).
 * Safe to create multiple times on different screens.
 */
void now_playing_widget_create(lv_obj_t *parent);
void now_playing_widget_destroy(void);

/** Refresh from current app_state — call from on_event(UI_EVT_STATE_CHANGED). */
void now_playing_widget_update(void);

void now_playing_widget_apply_theme(void);

#ifdef __cplusplus
}
#endif