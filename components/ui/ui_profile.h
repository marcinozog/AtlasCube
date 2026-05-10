#pragma once

#include "defines.h"
#include "lvgl.h"
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

// Display dimensions derived from UI_PROFILE_* (defines.h)
#if defined(UI_PROFILE_240X296)
    #define DISPLAY_WIDTH   240
    #define DISPLAY_HEIGHT  296
#elif defined(UI_PROFILE_320x240)
    #define DISPLAY_WIDTH   320
    #define DISPLAY_HEIGHT  240
#elif defined(UI_PROFILE_MONO_128X64)
    #define DISPLAY_WIDTH   128
    #define DISPLAY_HEIGHT  64
#else
    #error "Unknown UI_PROFILE"
#endif

#ifdef __cplusplus
extern "C" {
#endif

// UI profile picked at compile time — describes screen dimensions,
// fonts and visibility flags for each screen. Lets us build the same
// UI code for different displays (color/mono, various resolutions)
// without a runtime JSON parser.
typedef struct {
    // screen_clock — absolute LCD coordinates (top-left origin)
    int16_t          clock_panel_x;
    int16_t          clock_panel_y;
    int16_t          clock_panel_w;
    int16_t          clock_panel_h;

    int16_t          clock_strip_x;
    int16_t          clock_strip_y;
    int16_t          clock_strip_w;
    int16_t          clock_strip_h;
    int16_t          clock_strip_label_w;          // station/title label width inside strip
    int16_t          clock_strip_station_y;        // Y offset inside strip
    int16_t          clock_strip_title_y;
    const lv_font_t *clock_strip_station_font;
    const lv_font_t *clock_strip_title_font;

    int16_t          clock_time_x;                 // time digit label position (HH:MM)
    int16_t          clock_time_y;
    const lv_font_t *clock_time_font;
    bool             clock_show_time;

    int16_t          clock_date_x;                 // date label position (Day YYYY-MM-DD)
    int16_t          clock_date_y;
    const lv_font_t *clock_date_font;
    bool             clock_show_date;

    bool             clock_show_strip;
    bool             clock_show_mode_indicator;
    bool             clock_show_event_indicator;
    int16_t          clock_mode_indic_x;       // mode_indicator widget position
    int16_t          clock_mode_indic_y;
    int16_t          clock_event_indic_x;      // event_indicator widget position
    int16_t          clock_event_indic_y;

    // screen_radio — absolute LCD coordinates (top-left origin)
    int16_t          radio_np_x;               // now-playing widget (station + title labels)
    int16_t          radio_np_y;
    bool             radio_show_np;

    int16_t          radio_state_x;            // "PLAYING / STOPPED / ..." label
    int16_t          radio_state_y;
    const lv_font_t *radio_state_font;

    int16_t          radio_audio_info_x;       // "44100 Hz  2ch  128kbps"
    int16_t          radio_audio_info_y;
    const lv_font_t *radio_audio_info_font;

    int16_t          radio_slider_x;
    int16_t          radio_slider_y;
    int16_t          radio_slider_w;
    int16_t          radio_slider_h;

    int16_t          radio_vol_label_x;
    int16_t          radio_vol_label_y;
    const lv_font_t *radio_vol_label_font;

    bool             radio_show_mode_indicator;
    bool             radio_show_clock;
    int16_t          radio_mode_indic_x;
    int16_t          radio_mode_indic_y;
    int16_t          radio_clock_widget_x;
    int16_t          radio_clock_widget_y;

    // screen_playlist
    int16_t          playlist_header_h;
    int16_t          playlist_item_h;
    int16_t          playlist_item_pad;
    int16_t          playlist_row_w;
    int16_t          playlist_row_label_w;
    int16_t          playlist_row_pad_left;
    int16_t          playlist_label_x;
    int16_t          playlist_label_y;
    int16_t          playlist_hint_x;
    int16_t          playlist_hint_y;
    const lv_font_t *playlist_header_font;
    const lv_font_t *playlist_row_font;

    // screen_bt — absolute LCD coordinates (top-left origin)
    int16_t          bt_circle_x;
    int16_t          bt_circle_y;
    int16_t          bt_circle_w;
    int16_t          bt_circle_h;
    const lv_font_t *bt_icon_font;             // BT symbol inside the circle
    bool             bt_show_circle;

    int16_t          bt_brand_x;               // "Bluetooth Audio" label
    int16_t          bt_brand_y;
    const lv_font_t *bt_brand_font;

    int16_t          bt_status_x;              // "Connected / Discoverable / ..."
    int16_t          bt_status_y;
    const lv_font_t *bt_status_font;

    int16_t          bt_slider_x;
    int16_t          bt_slider_y;
    int16_t          bt_slider_w;
    int16_t          bt_slider_h;

    int16_t          bt_vol_label_x;           // "Volume"
    int16_t          bt_vol_label_y;
    const lv_font_t *bt_vol_label_font;

    bool             bt_show_mode_indicator;
    bool             bt_show_clock;
    int16_t          bt_mode_indic_x;
    int16_t          bt_mode_indic_y;
    int16_t          bt_clock_widget_x;
    int16_t          bt_clock_widget_y;

    // screen_settings
    int16_t          settings_title_y;
    int16_t          settings_row_w;
    int16_t          settings_row_h;
    int16_t          settings_row1_y;
    int16_t          settings_row2_y;
    int16_t          settings_row3_y;
    int16_t          settings_slider_w;
    int16_t          settings_slider_h;
    int16_t          settings_hint_y;       // from bottom (negative)
    const lv_font_t *settings_title_font;
    const lv_font_t *settings_row_font;
    const lv_font_t *settings_value_font;
    const lv_font_t *settings_hint_font;

    // screen_equalizer
    int16_t          eq_title_y;
    int16_t          eq_info_y;        // "1kHz: +6dB" — active band info
    int16_t          eq_band_area_y;   // top of sliders (their upper edge)
    int16_t          eq_slider_h;      // vertical slider height
    int16_t          eq_slider_w;      // slider width/thickness (horizontal)
    int16_t          eq_band_w;        // band column width (slider + gap)
    int16_t          eq_hint_y;        // from bottom (negative)
    const lv_font_t *eq_title_font;
    const lv_font_t *eq_info_font;
    const lv_font_t *eq_freq_font;
    const lv_font_t *eq_hint_font;

    // screen_events
    int16_t          events_header_h;
    int16_t          events_item_h;          // row height (2 lines: title + meta)
    int16_t          events_item_pad;        // gap between rows
    int16_t          events_row_w;
    int16_t          events_row_label_w;
    int16_t          events_row_pad_hor;
    const lv_font_t *events_header_font;
    const lv_font_t *events_title_font;
    const lv_font_t *events_meta_font;

    // screen_wifi_ap
    int16_t          wifi_title_y;
    int16_t          wifi_card_w;
    int16_t          wifi_card_h;
    int16_t          wifi_card_y;           // offset from CENTER
    int16_t          wifi_card_pad_hor;
    int16_t          wifi_card_pad_ver;
    int16_t          wifi_row2_y;           // offset from row 1
    int16_t          wifi_row3_y;           // offset from row 1
    int16_t          wifi_hint_y;           // from bottom (negative)
    const lv_font_t *wifi_title_font;
    const lv_font_t *wifi_key_font;
    const lv_font_t *wifi_value_font;
    const lv_font_t *wifi_hint_font;
} ui_profile_t;

const ui_profile_t *ui_profile_get(void);

// Pointer to the "factory" profile — values compiled into firmware (read-only).
// Used for "Reset to defaults" and as a fallback during JSON load.
const ui_profile_t *ui_profile_defaults(void);

// Overwrite the mutable runtime with the given profile (e.g. after a web patch).
// The pointer returned by ui_profile_get() stays valid — same struct.
void ui_profile_set(const ui_profile_t *src);

// Reset the entire profile to defaults.
void ui_profile_reset(void);

// Persistence: /spiffs/ui_profile.json. The file stores per-field overrides;
// missing fields → defaults. Format matches what /api/ui/profile/* returns
// and accepts (per-screen sections).
esp_err_t ui_profile_load_from_file(void);
esp_err_t ui_profile_save_to_file(void);

// ── per-section JSON helpers (used by http_server) ──────────────────────────
// `cJSON *` is returned as void* to avoid pulling cJSON.h into ui_profile.h —
// http_server already has cJSON.h, but other users of ui_profile.h might not.

void *ui_profile_dump_clock(void);          // returns cJSON object (caller: cJSON_Delete)
void  ui_profile_patch_clock(const void *obj); // accepts cJSON object — patch runtime

void *ui_profile_dump_bt(void);
void  ui_profile_patch_bt(const void *obj);

void *ui_profile_dump_radio(void);
void  ui_profile_patch_radio(const void *obj);

#ifdef __cplusplus
}
#endif
