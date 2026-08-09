#pragma once
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Album cover for the SD music player: an optional "cover.bin" (LVGL .bin, the
// format the web UI's uploader writes) sitting in the folder of the playing
// track. Nothing is shown while the file is missing, so a card without covers
// looks exactly as it did before. All functions must be called from the LVGL
// task.
void sd_cover_widget_create(lv_obj_t *parent, int x, int y, int size);

// Re-read the cover when the playing folder changed. Cheap to call on every
// state change: the file is only touched when the folder is a different one.
void sd_cover_widget_update(void);

void sd_cover_widget_destroy(void);

#ifdef __cplusplus
}
#endif
