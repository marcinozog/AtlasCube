#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Apply the dithered gradient background to `obj`. Intended to be called once
// on lv_scr_act() at UI startup: all screens are built on that single screen
// object and lv_obj_clean() only removes children, so the background persists
// across navigations. On mono panels this is a no-op (a gradient dithers to
// 1-bit noise) — the per-screen solid background is left in place.
void ui_background_apply(lv_obj_t *obj);

// Forget any loaded SD wallpaper so the next ui_background_apply() re-reads it
// from settings (display.wallpaper_path). Call only from the LVGL task. On mono
// panels this is a no-op.
void ui_background_reload_wallpaper(void);

#ifdef __cplusplus
}
#endif
