#pragma once
#include <stdint.h>
#include "esp_err.h"
#include "app_state.h"   // ui_theme_t

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t bg_primary;     // screen background
    uint32_t bg_secondary;   // strip / card background
    uint32_t text_primary;   // clock digits, main labels
    uint32_t text_secondary; // date, station name in strip, vol%
    uint32_t text_muted;     // audio info, ICY title in strip, "Syncing..."
    uint32_t accent;         // station name in radio, slider fill
    uint32_t bt_brand;       // BT circle, "Bluetooth Audio" label, BT slider fill
    uint32_t status_ok;      // Connected/Playing status etc. (semantic green)
} ui_theme_colors_t;

const ui_theme_colors_t *theme_get(void);
void       theme_set(ui_theme_t t);
ui_theme_t theme_current(void);

// Access to palettes by id (not just the active one) — for API-side serialization
const ui_theme_colors_t *theme_palette_get(ui_theme_t t);
void                     theme_palette_set(ui_theme_t t, const ui_theme_colors_t *c);
void                     theme_palette_reset(ui_theme_t t);

// Persistence — separate file /spiffs/theme.json
esp_err_t theme_load_from_file(void);
esp_err_t theme_save_to_file(void);

#ifdef __cplusplus
}
#endif