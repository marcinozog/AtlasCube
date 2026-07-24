#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Load an LVGL v9 RGB565 .bin (the scripts/img2lvgl.py format) from `path` into
// a newly allocated PSRAM-backed image descriptor. Returns NULL on any error
// (missing file, bad header, wrong colour format, alloc fail, or — when
// require_w/require_h are non-zero — a size mismatch). The caller owns the
// result and frees it with lv_bin_image_free(). Does not mount the SD card;
// call sdcard_init() first if the path lives on SD.
lv_image_dsc_t *lv_bin_image_load(const char *path, int require_w, int require_h);

// Like lv_bin_image_load() but bilinearly resamples the image to dst_w x dst_h
// (RGB565). Use it when the on-screen size is driven by a widget's geometry
// rather than the file's native pixels — e.g. a slider knob that scales with the
// slider box. Returns NULL on error; the caller frees the result with
// lv_bin_image_free(). dst_w/dst_h must be > 0.
lv_image_dsc_t *lv_bin_image_load_scaled(const char *path, int dst_w, int dst_h);

// Free a descriptor from lv_bin_image_load() and its pixel buffer, dropping the
// LVGL image cache for it first. Call only from the LVGL task.
void lv_bin_image_free(lv_image_dsc_t *dsc);

#ifdef __cplusplus
}
#endif
