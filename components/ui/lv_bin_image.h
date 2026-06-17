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

// Free a descriptor from lv_bin_image_load() and its pixel buffer, dropping the
// LVGL image cache for it first. Call only from the LVGL task.
void lv_bin_image_free(lv_image_dsc_t *dsc);

#ifdef __cplusplus
}
#endif
