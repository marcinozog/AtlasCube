#pragma once

#include "lvgl.h"
#include "ui_profile.h"
#include "control_action.h"

#ifdef __cplusplus
extern "C" {
#endif

void touch_hotspots_widget_create(lv_obj_t *parent, control_source_t source,
                                  const ui_touch_hotspot_t *hotspots);
void touch_hotspots_widget_destroy(void);

#ifdef __cplusplus
}
#endif
