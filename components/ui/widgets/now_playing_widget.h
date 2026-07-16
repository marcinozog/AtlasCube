#pragma once
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * "Now playing" widget: station name (always) + ICY title below (when available).
 * Safe to create multiple times on different screens.
 *
 * @param align        horizontal text alignment applied to both station and title
 *                     labels (LV_TEXT_ALIGN_LEFT / _CENTER / _RIGHT).
 * @param station_font font for the station-name line
 * @param show_title   create and update the ICY-title line
 * @param title_font   font for the ICY-title line (placed below the station line)
 */
void now_playing_widget_create(lv_obj_t *parent, int x, int y, lv_text_align_t align,
                               const lv_font_t *station_font,
                               bool show_title,
                               const lv_font_t *title_font);
void now_playing_widget_destroy(void);

/** Refresh from current app_state — call from on_event(UI_EVT_STATE_CHANGED). */
void now_playing_widget_update(void);

void now_playing_widget_apply_theme(void);

#ifdef __cplusplus
}
#endif
