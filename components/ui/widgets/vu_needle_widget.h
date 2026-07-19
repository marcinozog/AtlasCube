#pragma once

#include "lvgl.h"
#include "media_control.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Analogue needle VU meter: two independently positioned meters (left/right
// channel), each just a pivoting needle with an optional thin 1 px frame. The
// meter face (scale, markings) is expected to come from the wallpaper behind —
// the widget itself never fills its background. Fed by the per-channel RMS
// tracked in audio_levels; no FFT, so much lighter than the spectrum VU.
//
// X/Y are the top-left corner of each meter rectangle in absolute LCD pixels;
// the needle pivots at the bottom-centre and sweeps ~±45°. Either meter can be
// hidden (e.g. a single mono dial on the wallpaper). Needle colour comes from
// the theme's vu_bar, frame colour from vu_bg.
//
// One instance at a time (radio or SD-player screen — only one is shown at
// once). create() spins up the refresh timer; destroy() tears it down.
//
// owner ties the meters to their screen's source: when another source holds
// the audio path (media_source_current() != owner) the needles fall to rest
// instead of visualizing the foreign programme.
void vu_needle_widget_create(lv_obj_t *parent,
                             bool show_l, int16_t l_x, int16_t l_y, int16_t l_w, int16_t l_h,
                             bool show_r, int16_t r_x, int16_t r_y, int16_t r_w, int16_t r_h,
                             bool frame, media_source_t owner);
void vu_needle_widget_destroy(void);

// Recolour needle + frame from the active theme. Safe to call when not created.
void vu_needle_widget_apply_theme(void);

#ifdef __cplusplus
}
#endif
