#pragma once

#include "lvgl.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Creates the clock widget as a child of the given container.
 * Can be called multiple times on different screens — each screen
 * creates its own instance and destroys it on destroy().
 *
 * @param parent    LVGL container (e.g. lv_scr_act() or any lv_obj)
 * @param large     true = large font 48 (screen_clock),
 *                  false = small 14/18 (status bar on another screen)
 */
void clock_widget_create(lv_obj_t *parent, bool large);

/**
 * Destroys the widget and stops the LVGL timer.
 * Call from destroy() of the screen that earlier called clock_widget_create().
 */
void clock_widget_destroy(void);

/**
 * Forces a refresh of the displayed time.
 * Normally called automatically by the internal LVGL timer, but you can
 * call it manually e.g. in response to UI_EVT_STATE_CHANGED (to refresh
 * immediately after NTP sync).
 */
void clock_widget_tick(void);

void clock_widget_apply_theme(void);

#ifdef __cplusplus
}
#endif