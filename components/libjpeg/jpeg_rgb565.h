#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Decode a JPEG into a freshly allocated dst_w x dst_h RGB565 buffer in PSRAM,
// which the caller frees with heap_caps_free().
//
// The image is scaled to COVER the target (the library's DCT-domain N/8 scaling
// gets close, capped at its 2x upscale) and then centre-cropped while the rows
// stream through an RGB888 line buffer — nothing full-size is ever held. A
// source that still falls short of the target keeps a black letterbox around it.
//
// Returns NULL on any failure; when `err` is non-NULL it receives a short
// printable reason ("jpeg: Unsupported marker type", "progressive 4000x3000 too
// big: needs ~14 MB, ~4 MB free") for a status line or a log.
uint16_t *jpeg_file_to_rgb565(const char *path, int dst_w, int dst_h,
                              char *err, size_t err_cap);

// Same, for a JPEG already held in memory.
uint16_t *jpeg_mem_to_rgb565(const uint8_t *buf, size_t len, int dst_w, int dst_h,
                             char *err, size_t err_cap);

#ifdef __cplusplus
}
#endif
