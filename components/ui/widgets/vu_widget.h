#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Real-audio spectrum VU meter. A compact, OPAQUE bar display in its own small
// container, fed by an FFT of the post-DSP PCM tapped in audio_levels. Because it
// is contained and opaque, only its own rectangle redraws — unlike the
// full-screen, transparent ui_anim_bg background, which forced the whole UI to
// recomposite every frame. The FFT runs on the LVGL task (in the widget's own
// timer), so the audio hot path stays a cheap ring append.
//
// One instance at a time (radio screen). create() spins up the refresh timer;
// destroy() tears it and the bars down.
void vu_widget_create(lv_obj_t *parent, int x, int y, int w, int h);
void vu_widget_destroy(void);

// Recolour bars + container from the active theme. Safe to call when not created.
void vu_widget_apply_theme(void);

#ifdef __cplusplus
}
#endif
