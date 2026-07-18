#pragma once

#include "defines.h"
#include "lvgl.h"
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

// Display dimensions and touch orientation derived from UI_PROFILE_* (defines.h).
// TOUCH_* are applied in this order: swap_xy → mirror_x → mirror_y (in display frame).
#if defined(UI_PROFILE_240X296)
    #define DISPLAY_WIDTH   240
    #define DISPLAY_HEIGHT  296
    #define TOUCH_SWAP_XY   0
    #define TOUCH_MIRROR_X  1
    #define TOUCH_MIRROR_Y  1
#elif defined(UI_PROFILE_320x240)
    #define DISPLAY_WIDTH   320
    #define DISPLAY_HEIGHT  240
    #define TOUCH_SWAP_XY   1
    #define TOUCH_MIRROR_X  1
    #define TOUCH_MIRROR_Y  0
    // XPT2046 raw ADC spans from a bring-up session on one ILI9341+XPT2046 panel
    // (swaps: RAW_X bounds the raw-Y channel → width, RAW_Y bounds raw-X → height).
    // Direction (MIN>MAX = inverted) is a best-guess from blind corner presses —
    // verify on a live screen and swap MIN<->MAX per axis if a tap lands mirrored.
    #define TOUCH_RAW_X_MIN 3800
    #define TOUCH_RAW_X_MAX 400
    #define TOUCH_RAW_Y_MIN 3750
    #define TOUCH_RAW_Y_MAX 500
#elif defined(UI_PROFILE_480x320)
    #define DISPLAY_WIDTH   480
    #define DISPLAY_HEIGHT  320
    #define TOUCH_SWAP_XY   1
    #define TOUCH_MIRROR_X  1
    #define TOUCH_MIRROR_Y  0
#elif defined(UI_PROFILE_MONO_128X64)
    #define DISPLAY_WIDTH   128
    #define DISPLAY_HEIGHT  64
    #define TOUCH_SWAP_XY   0
    #define TOUCH_MIRROR_X  0
    #define TOUCH_MIRROR_Y  0
#elif defined(UI_PROFILE_MONO_256X64)
    #define DISPLAY_WIDTH   256
    #define DISPLAY_HEIGHT  64
    #define TOUCH_SWAP_XY   0
    #define TOUCH_MIRROR_X  0
    #define TOUCH_MIRROR_Y  0
#else
    #error "Unknown UI_PROFILE"
#endif

// XPT2046 resistive-touch calibration: raw 12-bit ADC → pixel range, applied in
// touch.c before swap/mirror/flip. These spans suit a typical ILI9341/ST7796
// shield; to tune for a specific panel, #define TOUCH_RAW_* inside that profile
// above (this block only fills in what a profile didn't override).
#ifndef TOUCH_RAW_X_MIN
    #define TOUCH_RAW_X_MIN 200
#endif
#ifndef TOUCH_RAW_X_MAX
    #define TOUCH_RAW_X_MAX 3900
#endif
#ifndef TOUCH_RAW_Y_MIN
    #define TOUCH_RAW_Y_MIN 200
#endif
#ifndef TOUCH_RAW_Y_MAX
    #define TOUCH_RAW_Y_MAX 3900
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define UI_TOUCH_HOTSPOT_COUNT 6

typedef struct {
    bool    enabled;
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
    int16_t action;
    int16_t radius;   // corner radius in percent: 0 rectangle, 100 pill/circle
} ui_touch_hotspot_t;

// UI profile picked at compile time — describes screen dimensions,
// fonts and visibility flags for each screen. Lets us build the same
// UI code for different displays (color/mono, various resolutions)
// without a runtime JSON parser.
typedef struct {
    // clock face (screen_home) — absolute LCD coordinates (top-left origin)
    int16_t          clock_panel_x;
    int16_t          clock_panel_y;
    int16_t          clock_panel_w;
    int16_t          clock_panel_h;

    int16_t          clock_strip_x;
    int16_t          clock_strip_y;
    int16_t          clock_strip_w;
    int16_t          clock_strip_h;
    int16_t          clock_strip_bg_opa;           // background opacity, 0-100 %
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


    // Network info (IP + "<hostname>.local") — a clock screen element, toggled
    // and positioned in the layout editor like the date/strip.
    bool             clock_show_netinfo;
    int16_t          clock_netinfo_x;
    int16_t          clock_netinfo_y;
    const lv_font_t *clock_netinfo_font;

    bool             clock_show_strip;
    bool             clock_show_mode_indicator;
    bool             clock_show_event_indicator;
    int16_t          clock_mode_indic_x;       // mode_indicator widget position
    int16_t          clock_mode_indic_y;
    int16_t          clock_event_indic_x;      // event_indicator widget position
    int16_t          clock_event_indic_y;

    // Calendar widget — next upcoming EV_CALENDAR event for today (mirror of the
    // phone calendar). Hidden when nothing is upcoming; off by default.
    bool             clock_show_calendar;
    int16_t          clock_calendar_x;
    int16_t          clock_calendar_y;
    int16_t          clock_calendar_w;         // label width for scroll (0 → full width)
    const lv_font_t *clock_calendar_font;
    bool             clock_show_weather;
    int16_t          clock_weather_x;
    int16_t          clock_weather_y;
    int16_t          clock_weather_w;
    const lv_font_t *clock_weather_font;

    // screen_radio — absolute LCD coordinates (top-left origin)
    int16_t          radio_np_x;               // now-playing widget (station + title labels)
    int16_t          radio_np_y;
    bool             radio_show_np;
    bool             radio_show_np_title;
    bool             radio_show_station_icon;
    int16_t          radio_station_icon_x;     // station artwork top-left position
    int16_t          radio_station_icon_y;
    int16_t          radio_station_icon_size;  // displayed station artwork size, 16..64 px
    const lv_font_t *radio_np_station_font;    // now-playing station-name line
    const lv_font_t *radio_np_title_font;      // now-playing ICY-title line

    bool             radio_show_playback_status;
    int16_t          radio_state_y;            // "PLAYING / STOPPED / ..." label (centered)
    const lv_font_t *radio_state_font;

    int16_t          radio_audio_info_y;       // "44100 Hz  2ch  128kbps   VOL: 42%" (centered)
    const lv_font_t *radio_audio_info_font;

    bool             radio_show_mode_indicator;
    bool             radio_show_clock;
    bool             radio_show_event_indicator;
    int16_t          radio_mode_indic_x;
    int16_t          radio_mode_indic_y;
    int16_t          radio_clock_widget_x;
    int16_t          radio_clock_widget_y;
    const lv_font_t *radio_clock_font;         // "HH:MM" clock widget on radio screen
    int16_t          radio_event_indic_x;
    int16_t          radio_event_indic_y;

    bool             radio_show_vu;            // real-audio FFT spectrum widget
    int16_t          radio_vu_x;               // container top-left + size (LCD px)
    int16_t          radio_vu_y;
    int16_t          radio_vu_w;
    int16_t          radio_vu_h;
    bool             radio_vu_transparent;     // no bg fill: bars sit on wallpaper/gradient
    bool             radio_needle_show_l;      // analogue needle VU, left channel meter
    bool             radio_needle_show_r;      // right channel meter (each independent)
    bool             radio_needle_frame;       // thin 1 px frame around each meter
    int16_t          radio_needle_l_x;         // per-meter top-left + size (LCD px)
    int16_t          radio_needle_l_y;
    int16_t          radio_needle_l_w;
    int16_t          radio_needle_l_h;
    int16_t          radio_needle_r_x;
    int16_t          radio_needle_r_y;
    int16_t          radio_needle_r_w;
    int16_t          radio_needle_r_h;
    bool             radio_show_cassette;      // legacy JSON name: show animated wheels overlay
    int16_t          radio_animation_style;    // 0 cassette reels, 1 car rims
    bool             radio_show_wheel_left;
    bool             radio_show_wheel_right;
    int16_t          radio_cassette_l_x;        // top-left + square size, LCD px
    int16_t          radio_cassette_l_y;
    int16_t          radio_cassette_l_size;
    int16_t          radio_cassette_r_x;
    int16_t          radio_cassette_r_y;
    int16_t          radio_cassette_r_size;
    bool             radio_show_weather;
    int16_t          radio_weather_x;
    int16_t          radio_weather_y;
    int16_t          radio_weather_w;
    const lv_font_t *radio_weather_font;
    bool             radio_show_ctrl_overlay;   // tap-anywhere transport/volume overlay
    ui_touch_hotspot_t radio_touch_hotspots[UI_TOUCH_HOTSPOT_COUNT];

    // screen_sd_player — absolute LCD coordinates (top-left origin). The three
    // text rows are horizontally centered (Y only). Mirrors screen_radio.
    int16_t          sd_title_y;               // track title (ID3 / file name)
    const lv_font_t *sd_title_font;
    int16_t          sd_folder_y;              // "<folder>   idx/count"
    const lv_font_t *sd_folder_font;
    bool             sd_show_folder;           // folder/index row
    int16_t          sd_info_y;                // "VOL: n%   PAUSED   SHUFFLE ..."
    const lv_font_t *sd_info_font;
    bool             sd_show_info;             // VOL/mode row
    bool             sd_show_time;             // "elapsed / total" row (needs a spare line; off on mono)
    bool             sd_show_bar;              // read-only progress bar under the time row (needs a duration)
    int16_t          sd_bar_w;                 // progress bar size (LCD px); centered under the time row
    int16_t          sd_bar_h;

    bool             sd_show_mode_indicator;
    bool             sd_show_clock;
    bool             sd_show_event_indicator;
    int16_t          sd_mode_indic_x;
    int16_t          sd_mode_indic_y;
    int16_t          sd_clock_widget_x;
    int16_t          sd_clock_widget_y;
    const lv_font_t *sd_clock_font;            // "HH:MM" clock widget on SD screen
    int16_t          sd_event_indic_x;
    int16_t          sd_event_indic_y;

    bool             sd_show_vu;                // real-audio FFT spectrum widget
    int16_t          sd_vu_x;                   // container top-left + size (LCD px)
    int16_t          sd_vu_y;
    int16_t          sd_vu_w;
    int16_t          sd_vu_h;
    bool             sd_vu_transparent;         // no bg fill: bars sit on wallpaper/gradient
    bool             sd_needle_show_l;          // analogue needle VU, left channel meter
    bool             sd_needle_show_r;          // right channel meter (each independent)
    bool             sd_needle_frame;           // thin 1 px frame around each meter
    int16_t          sd_needle_l_x;             // per-meter top-left + size (LCD px)
    int16_t          sd_needle_l_y;
    int16_t          sd_needle_l_w;
    int16_t          sd_needle_l_h;
    int16_t          sd_needle_r_x;
    int16_t          sd_needle_r_y;
    int16_t          sd_needle_r_w;
    int16_t          sd_needle_r_h;
    bool             sd_show_cassette;         // legacy JSON name: show animated wheels overlay
    int16_t          sd_animation_style;       // 0 cassette reels, 1 car rims
    bool             sd_show_wheel_left;
    bool             sd_show_wheel_right;
    int16_t          sd_cassette_l_x;           // top-left + square size, LCD px
    int16_t          sd_cassette_l_y;
    int16_t          sd_cassette_l_size;
    int16_t          sd_cassette_r_x;
    int16_t          sd_cassette_r_y;
    int16_t          sd_cassette_r_size;
    bool             sd_show_weather;
    int16_t          sd_weather_x;
    int16_t          sd_weather_y;
    int16_t          sd_weather_w;
    const lv_font_t *sd_weather_font;
    bool             sd_show_ctrl_overlay;      // tap-anywhere transport/volume overlay
    ui_touch_hotspot_t sd_touch_hotspots[UI_TOUCH_HOTSPOT_COUNT];

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

    const lv_font_t *bt_vol_label_font;        // "VOL: NN%", positioned below time label

    bool             bt_show_mode_indicator;
    bool             bt_show_clock;
    int16_t          bt_mode_indic_x;
    int16_t          bt_mode_indic_y;
    int16_t          bt_clock_widget_x;
    int16_t          bt_clock_widget_y;
    const lv_font_t *bt_clock_font;            // "HH:MM" clock widget on BT screen

    // Track metadata (sent over UART by BT module)
    int16_t          bt_title_x;
    int16_t          bt_title_y;
    int16_t          bt_title_w;               // width for scroll
    const lv_font_t *bt_title_font;
    int16_t          bt_artist_x;
    int16_t          bt_artist_y;
    int16_t          bt_artist_w;
    const lv_font_t *bt_artist_font;
    int16_t          bt_time_x;
    int16_t          bt_time_y;
    const lv_font_t *bt_time_font;

    // screen_settings
    int16_t          settings_title_y;
    int16_t          settings_row_w;
    int16_t          settings_row_h;
    int16_t          settings_row_pad;      // inner card padding (px)
    int16_t          settings_row1_y;
    int16_t          settings_row2_y;
    int16_t          settings_row3_y;
    int16_t          settings_slider_w;
    int16_t          settings_slider_h;
    bool             settings_show_slider;  // draw value slider (off on touchless/short panels)
    int16_t          settings_hint_y;       // from bottom (negative)
    bool             settings_title_in_list; // title scrolls with the list (small panels)
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

// Persistence: /config/ui_profile.json. The file stores per-field overrides;
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

void *ui_profile_dump_sd(void);
void  ui_profile_patch_sd(const void *obj);

#ifdef __cplusplus
}
#endif
