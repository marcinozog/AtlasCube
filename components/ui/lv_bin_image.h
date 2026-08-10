#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Load an LVGL v9 .bin (the scripts/img2lvgl.py format) from `path` into a
// newly allocated PSRAM-backed image descriptor. Both RGB565 and RGB565A8 are
// accepted — the latter is what the Assets-tab uploader writes for artwork with
// transparency, and the descriptor keeps that format. Returns NULL on any error
// (missing file, bad header, wrong colour format, alloc fail, or — when
// require_w/require_h are non-zero — a size mismatch). The caller owns the
// result and frees it with lv_bin_image_free(). Does not mount the SD card;
// call sdcard_init() first if the path lives on SD.
lv_image_dsc_t *lv_bin_image_load(const char *path, int require_w, int require_h);

// Wrap an RGB565 pixel buffer the caller allocated (heap_caps_malloc / the
// libjpeg decoder's output) into a descriptor LVGL can draw. The descriptor
// TAKES OWNERSHIP: release both with lv_bin_image_free(), exactly like a loaded
// one, so a widget can hold artwork from a file and from a decoder without
// caring which it got. Returns NULL on a bad size or an alloc failure (the
// buffer is then the caller's to free).
lv_image_dsc_t *lv_bin_image_wrap_rgb565(uint16_t *buf, int w, int h);

// Like lv_bin_image_load() but bilinearly resamples the image to dst_w x dst_h
// (RGB565). Use it when the on-screen size is driven by a widget's geometry
// rather than the file's native pixels — e.g. a slider knob that scales with the
// slider box. Returns NULL on error; the caller frees the result with
// lv_bin_image_free(). dst_w/dst_h must be > 0.
lv_image_dsc_t *lv_bin_image_load_scaled(const char *path, int dst_w, int dst_h);

// Bilinearly resample an already-loaded descriptor to dst_w x dst_h. Consumes
// `src` (frees it, or returns it unchanged when it is already the target size),
// so the caller keeps only the returned descriptor and frees it with
// lv_bin_image_free(). Returns NULL on error (src is freed in that case too).
// Use it when the target size depends on the image's own aspect ratio, which is
// only known after loading. dst_w/dst_h must be > 0.
lv_image_dsc_t *lv_bin_image_scale(lv_image_dsc_t *src, int dst_w, int dst_h);

// Bilinearly resample a descriptor the caller does NOT own into a fresh one —
// e.g. an internet asset slot, whose pixels are shared by every widget bound to
// it and must outlive them. `src` is left untouched; the result is the caller's
// to free with lv_bin_image_free(). Copies even when the size already matches,
// so the caller always owns independent pixels. Understands RGB565 and
// RGB565A8 (the alpha plane is resampled too) and keeps the source's format.
lv_image_dsc_t *lv_bin_image_scale_copy(const lv_image_dsc_t *src, int dst_w, int dst_h);

// Free a descriptor from lv_bin_image_load() and its pixel buffer, dropping the
// LVGL image cache for it first. Call only from the LVGL task.
void lv_bin_image_free(lv_image_dsc_t *dsc);

#ifdef __cplusplus
}
#endif
