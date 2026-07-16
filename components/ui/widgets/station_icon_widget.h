#pragma once
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Loads the current playlist entry's optional SD-card icon. All functions must
// be called from the LVGL task.
void station_icon_widget_create(lv_obj_t *parent, int x, int y, int size);
void station_icon_widget_update(void);
void station_icon_widget_destroy(void);

#ifdef __cplusplus
}
#endif
