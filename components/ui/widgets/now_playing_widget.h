#pragma once
#include "lvgl.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * "Now playing" widget: two INDEPENDENT single-line labels — station name and
 * ICY title — each with its own box (x = left edge, w = box width). Text is
 * centered in its box and scrolls when it doesn't fit. Each label carries its
 * own background plate (label_bg_opa).
 *
 * @param station_x/y/w box of the station-name line
 * @param station_font  font for the station-name line
 * @param show_title    create and update the ICY-title line
 * @param title_x/y/w   box of the ICY-title line
 * @param title_font    font for the ICY-title line
 */
void now_playing_widget_create(lv_obj_t *parent,
                               int station_x, int station_y, int station_w,
                               const lv_font_t *station_font,
                               bool show_title,
                               int title_x, int title_y, int title_w,
                               const lv_font_t *title_font,
                               int label_bg_opa);
void now_playing_widget_destroy(void);

/** Refresh from current app_state — call from on_event(UI_EVT_STATE_CHANGED). */
void now_playing_widget_update(void);

void now_playing_widget_apply_theme(void);

#ifdef __cplusplus
}
#endif
