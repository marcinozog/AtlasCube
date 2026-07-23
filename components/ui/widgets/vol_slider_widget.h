#pragma once

#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Draggable on-screen volume slider (0-100 %). Geometry is a rectangle in
// absolute LCD pixels; h > w renders vertical (LVGL picks the orientation
// from the geometry). One instance at a time (the active screen owns it).
//
// bt=false drives the main output: the level is applied live while dragging
// (audio_engine_set_volume) and persisted once on release via
// settings_set_volume() — no settings write per drag event. bt=true drives
// the BT channel and applies on release only: every BT volume change is an
// AT command to the module over UART, so a live drag would flood the link.
void vol_slider_widget_create(lv_obj_t *parent, int16_t x, int16_t y,
                              int16_t w, int16_t h, bool bt);
void vol_slider_widget_destroy(void);

// Sync the knob with app_state (encoder / WS / Android changes). Skipped
// while the user is dragging so an in-flight gesture isn't snapped back.
void vol_slider_widget_update(void);

// Recolour from the active theme. Safe to call when not created.
void vol_slider_widget_apply_theme(void);

#ifdef __cplusplus
}
#endif
