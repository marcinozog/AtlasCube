#pragma once
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Custom fonts shipped in components/ui/fonts/*.c.
// Built-in LVGL fonts (e.g. lv_font_montserrat_28) are declared by lvgl.h
// when enabled via Kconfig (CONFIG_LV_FONT_MONTSERRAT_*).
//
// The _eu text fonts cover ASCII plus Latin-1 Supplement (0x00A0-0x00FF),
// Latin Extended-A (0x0100-0x017F) and the Romanian comma-below forms
// (0x0218-0x021B) — i.e. every language written in Latin script in Europe,
// not just Polish. Regenerate them with scripts/gen_fonts.ps1.
LV_FONT_DECLARE(lv_font_montserrat_8_eu);
LV_FONT_DECLARE(lv_font_montserrat_10_eu);
LV_FONT_DECLARE(lv_font_montserrat_12_eu);
LV_FONT_DECLARE(lv_font_montserrat_14_eu);
LV_FONT_DECLARE(lv_font_montserrat_18_eu);
LV_FONT_DECLARE(lv_font_montserrat_24_eu);
// Replaces LVGL's built-in montserrat_48 and therefore also carries the full
// LV_SYMBOL_* FontAwesome set — the overlays render their button glyphs at 48.
LV_FONT_DECLARE(lv_font_montserrat_48_eu);
LV_FONT_DECLARE(lv_font_montserrat_72);
LV_FONT_DECLARE(lv_font_montserrat_80);
LV_FONT_DECLARE(lv_font_montserrat_96);
LV_FONT_DECLARE(lv_font_montserrat_120);
// Weather Icons (erikflowers, OFL) glyphs for the weather widget — icon-only,
// deliberately NOT in the registry table (useless as a text font in profiles).
LV_FONT_DECLARE(lv_font_weather_20);

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
