#pragma once

#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Draggable on-screen volume slider (0-100 %). Geometry is a rectangle in
// absolute LCD pixels; `vertical` picks the orientation explicitly. When the
// box contradicts the chosen orientation (e.g. vertical with w > h) the two
// dimensions are swapped at create time — LVGL sliders derive the drag axis
// from the geometry, so the box must always agree with the orientation.
// One instance at a time (the active screen owns it).
//
// knob_only hides the track and fill, leaving just the draggable knob — for
// wallpapers that paint their own slider artwork. The (invisible) box still
// defines the drag range, and its thickness sizes the knob.
//
// knob_image is an optional path to an RGB565 .bin on SD (scripts/img2lvgl.py
// format) drawn on the knob. The slider's thickness (cross axis: h for
// horizontal, w for vertical) sets the knob size up to a 64 px cap; the other
// axis follows the image's aspect ratio (also capped), so non-square art keeps
// its proportions. Resizing the slider box resizes the knob up to that cap (see
// KNOB_IMAGE_MAX_SIDE for the LVGL layer reason). NULL or "" keeps the plain
// themed colour knob. No alpha channel — the image is a solid rectangle. Loaded
// via lv_bin_image; the SD card must already be mounted.
//
// bt=false drives the main output: the level is applied live while dragging
// (audio_engine_set_volume) and persisted once on release via
// settings_set_volume() — no settings write per drag event. bt=true drives
// the BT channel and applies on release only: every BT volume change is an
// AT command to the module over UART, so a live drag would flood the link.
//
// A sideways drag across the slider is the screen's exit swipe, not a volume
// change: it navigates and the level returns to where it was when the finger
// landed. Vertical (or along-axis) drags stay the user's volume gesture.
//
// vol_max (1..100) remaps the slider's full travel onto the effective output
// range: full deflection = vol_max %, so a room-loud panel can use the whole
// slider for fine control instead of the bottom third. The value stored in
// app_state / persisted stays the true 0-100 %, so other sliders, web and
// Android remain honest — only this control's mapping is compressed. 100 (or
// any out-of-range value) means no scaling — the legacy 1:1 behaviour.
void vol_slider_widget_create(lv_obj_t *parent, int16_t x, int16_t y,
                              int16_t w, int16_t h, bool vertical,
                              bool knob_only, bool bt, const char *knob_image,
                              int vol_max);
void vol_slider_widget_destroy(void);

// Sync the knob with app_state (encoder / WS / Android changes). Skipped
// while the user is dragging so an in-flight gesture isn't snapped back.
void vol_slider_widget_update(void);

// Recolour from the active theme. Safe to call when not created.
void vol_slider_widget_apply_theme(void);

#ifdef __cplusplus
}
#endif
