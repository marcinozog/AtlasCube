#pragma once
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Custom fonts shipped in components/ui/fonts/*.c.
// Built-in LVGL fonts (e.g. lv_font_montserrat_48) are declared by lvgl.h
// when enabled via Kconfig (CONFIG_LV_FONT_MONTSERRAT_*).
LV_FONT_DECLARE(lv_font_montserrat_12_pl);
LV_FONT_DECLARE(lv_font_montserrat_14_pl);
LV_FONT_DECLARE(lv_font_montserrat_18_pl);
LV_FONT_DECLARE(lv_font_montserrat_72);
LV_FONT_DECLARE(lv_font_montserrat_80);
LV_FONT_DECLARE(lv_font_montserrat_96);

// ── Font registry ───────────────────────────────────────────────────────────
// Maps id (string) ↔ font pointer, used for (de)serializing ui_profile
// to/from the JSON file on SPIFFS and the web UI.
//
// To add a new font (e.g. for a small mono LCD): add LV_FONT_DECLARE above
// and append a row to the table in ui_fonts.c — that's it.

const lv_font_t *ui_font_by_id(const char *id);     // NULL when unknown
const char       *ui_font_id(const lv_font_t *f);   // "" when unknown
int               ui_font_list_count(void);
const char       *ui_font_list_id(int i);           // NULL when out of range

#ifdef __cplusplus
}
#endif
