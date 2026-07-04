#pragma once
#include "lvgl.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Single-line agenda widget: shows the next upcoming EV_CALENDAR event for
 * today ("HH:MM  title"), scrolling when long. Hidden when nothing is upcoming.
 *
 * Fed by events_calendar_current() (calendar events mirror the phone calendar
 * and are never "fired" — this is their only on-screen surface). Place it
 * manually on the screens that want it; it refreshes itself every 30 s via an
 * internal lv_timer. calendar_widget_update() forces a refresh (e.g. on a
 * state change or in apply_theme).
 *
 * `w <= 0` falls back to the full display width, `font == NULL` to a default.
 */
void calendar_widget_create(lv_obj_t *parent, int16_t x, int16_t y,
                            int16_t w, const lv_font_t *font);
void calendar_widget_destroy(void);
void calendar_widget_update(void);
void calendar_widget_apply_theme(void);

#ifdef __cplusplus
}
#endif
