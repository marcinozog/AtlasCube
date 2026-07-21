#pragma once

#include "lvgl.h"
#include "media_control.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Stereo bar VU meter: two independently positioned bars (left/right channel),
// each a single bar that grows with the channel's level. Fed by the per-channel
// RMS tracked in audio_levels (same tap as the needle VU) — no FFT, so much
// lighter than the spectrum VU. The RMS→dB→AGC→ballistics pipeline is shared
// with the needle VU; only the rendering differs (a bar instead of a needle).
//
// Each bar has its own rectangle in absolute LCD pixels. horizontal=false grows
// the bar from the bottom up; horizontal=true grows it from the left edge. Either
// side can be hidden. With peak=true a thin marker holds the recent maximum and
// falls back slowly. Bar colour comes from the theme's vu_bar, frame/background
// from vu_bg; transparent=true drops the background so the bars sit on the
// wallpaper behind (still delta-bounded, ~2× the pixel work of an opaque fill).
//
// One instance at a time (radio or SD-player screen — only one is shown at once).
// create() spins up the refresh timer; destroy() tears it down.
//
// owner ties the bars to their screen's source: when another source holds the
// audio path (media_source_current() != owner) the bars fall to rest instead of
// visualizing the foreign programme.
void vu_stereo_widget_create(lv_obj_t *parent,
                             bool show_l, int16_t l_x, int16_t l_y, int16_t l_w, int16_t l_h,
                             bool show_r, int16_t r_x, int16_t r_y, int16_t r_w, int16_t r_h,
                             bool horizontal, bool frame, bool transparent,
                             bool peak, media_source_t owner);
void vu_stereo_widget_destroy(void);

// Recolour bars + frame from the active theme. Safe to call when not created.
void vu_stereo_widget_apply_theme(void);

#ifdef __cplusplus
}
#endif
