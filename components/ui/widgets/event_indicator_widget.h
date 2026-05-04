#pragma once
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Small indicator "🔔 N" — number of today's events that haven't fired yet.
 * Hidden when 0.
 *
 * Place it manually on the screens where you want it (e.g. next to
 * mode_indicator). The widget refreshes itself every 30 s via an internal
 * lv_timer — you don't have to call anything from on_event.
 * event_indicator_update() is available to force a refresh (e.g. in
 * apply_theme).
 */
void event_indicator_create(lv_obj_t *parent, lv_align_t align, int x, int y);
void event_indicator_destroy(void);
void event_indicator_update(void);
void event_indicator_apply_theme(void);

#ifdef __cplusplus
}
#endif
