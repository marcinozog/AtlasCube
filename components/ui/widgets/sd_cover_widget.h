#pragma once
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Album cover for the SD music player. Two sources, in this order: the folder's
// "cover.bin" (LVGL .bin — what the web uploader writes and what cover_art
// converts a cover.jpg into), and failing that the picture embedded in the
// playing track's own tag. Nothing is shown when a track has neither, so a card
// without artwork looks exactly as it did before. All functions must be called
// from the LVGL task.
void sd_cover_widget_create(lv_obj_t *parent, int x, int y, int size);

// Follow the playing track. Cheap to call on every state change: the card is
// only touched when the folder or the track actually changed.
void sd_cover_widget_update(void);

// Collect what cover_art finished in the background — a folder's newly written
// cover.bin, or artwork decoded out of the current track's tag (including the
// "this track has none" answer, which clears the previous one).
void sd_cover_widget_reload(void);

void sd_cover_widget_destroy(void);

#ifdef __cplusplus
}
#endif
