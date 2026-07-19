#pragma once

#include "lvgl.h"
#include "ui_events.h"   // ui_screen_id_t

#ifdef __cplusplus
extern "C" {
#endif

// Apply the shared background (wallpaper / gradient / solid) to `obj` for the
// given screen. All screens are built on the singleton lv_scr_act() and
// lv_obj_clean() only removes children, so ui_manager re-applies this after
// every navigation. The wallpaper path is resolved per screen: hub sections in
// ui_profile (clock/radio/sd/bt) may override the global default
// (display.wallpaper_path) with their own file or opt out with "none". On mono
// panels this is a no-op (a gradient dithers to 1-bit noise).
void ui_background_apply(lv_obj_t *obj, ui_screen_id_t screen);

// Forget every loaded SD wallpaper so the next ui_background_apply() re-reads
// them from SD (paths resolve from settings + ui_profile). Call only from the
// LVGL task. On mono panels this is a no-op.
void ui_background_reload_wallpaper(void);

#ifdef __cplusplus
}
#endif
