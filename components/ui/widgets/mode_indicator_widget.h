#pragma once
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void mode_indicator_create(lv_obj_t *parent, int x, int y);
void mode_indicator_destroy(void);
void mode_indicator_update(void);       // call from on_event(UI_EVT_STATE_CHANGED)
void mode_indicator_apply_theme(void);  // call from apply_theme()

#ifdef __cplusplus
}
#endif