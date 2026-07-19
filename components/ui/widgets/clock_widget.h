#pragma once

#include "lvgl.h"
#include "ui_label.h"
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
 * @param x         coordinates (interpreted per `align`: left/center/right)
 * @param y         coordinates (top edge)
 * @param font      font for the "HH:MM" label (per-screen, from ui_profile)
 * @param align     horizontal anchor — UI_ALIGN_CENTER keeps the time centered
 *                  on x as the proportional digits change width
 */
void clock_widget_create(lv_obj_t *parent, int x, int y, const lv_font_t *font,
                         ui_label_align_t align, int label_bg_opa);

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
