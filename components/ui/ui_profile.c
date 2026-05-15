#include "defines.h"
#include "ui_profile.h"
#include "fonts/ui_fonts.h"
#include "cJSON.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define UI_PROFILE_FILE "/spiffs/ui_profile.json"

static const char *TAG = "UI_PROFILE";

// Profile picked at compile time (Kconfig eventually). Stored in `k_defaults`
// as "factory" values — used for reset and as a fallback during JSON load.
// `s_runtime` is the live copy returned by ui_profile_get() — patchable from
// the web and persisted to SPIFFS.

#if defined(UI_PROFILE_MONO_128X64)

static const ui_profile_t k_defaults = {
    .clock_panel_x             = 0,
    .clock_panel_y             = 0,
    .clock_panel_w             = 128,
    .clock_panel_h             = 44,
    .clock_strip_x             = 0,
    .clock_strip_y             = 44,
    .clock_strip_w             = 128,
    .clock_strip_h             = 20,
    .clock_strip_label_w       = 124,
    .clock_strip_station_y     = 2,
    .clock_strip_title_y       = 10,
    .clock_strip_station_font  = &lv_font_montserrat_12_pl,
    .clock_strip_title_font    = &lv_font_montserrat_12_pl,
    .clock_time_x              = 50,
    .clock_time_y              = 8,
    .clock_time_font           = &lv_font_montserrat_18_pl,
    .clock_show_time           = true,
    .clock_date_x              = 0,
    .clock_date_y              = 0,
    .clock_date_font           = &lv_font_montserrat_12_pl,
    .clock_show_date           = false,
    .clock_show_strip          = true,
    .clock_show_mode_indicator = false,
    .clock_show_event_indicator = false,
    .clock_mode_indic_x        = 108,
    .clock_mode_indic_y        = 8,
    .clock_event_indic_x       = 90,
    .clock_event_indic_y       = 8,

    .radio_np_x                = 10,
    .radio_np_y                = 35,
    .radio_show_np             = true,
    .radio_state_x             = 39,
    .radio_state_y             = 16,
    .radio_state_font          = &lv_font_montserrat_12_pl,
    .radio_audio_info_x        = 4,
    .radio_audio_info_y        = 30,
    .radio_audio_info_font     = &lv_font_montserrat_12_pl,
    .radio_slider_x            = 14,
    .radio_slider_y            = 42,
    .radio_slider_w            = 100,
    .radio_slider_h            = 4,
    .radio_vol_label_x         = 53,
    .radio_vol_label_y         = 46,
    .radio_vol_label_font      = &lv_font_montserrat_12_pl,
    .radio_show_mode_indicator = false,
    .radio_show_clock          = false,
    .radio_mode_indic_x        = 108,
    .radio_mode_indic_y        = 8,
    .radio_clock_widget_x      = 39,
    .radio_clock_widget_y      = 0,

    .playlist_header_h         = 14,
    .playlist_item_h           = 12,
    .playlist_item_pad         = 1,
    .playlist_row_w            = 126,
    .playlist_row_label_w      = 118,
    .playlist_row_pad_left     = 4,
    .playlist_header_font      = &lv_font_montserrat_12_pl,
    .playlist_row_font         = &lv_font_montserrat_12_pl,

    // BT: on 128x64 the circle makes no sense — text only
    .bt_circle_x               = 0,
    .bt_circle_y               = 0,
    .bt_circle_w               = 0,
    .bt_circle_h               = 0,
    .bt_icon_font              = &lv_font_montserrat_12_pl,
    .bt_show_circle            = false,
    .bt_brand_x                = 4,            // overflow OK — text wider than 128
    .bt_brand_y                = 4,
    .bt_brand_font             = &lv_font_montserrat_14_pl,
    .bt_status_x               = 4,
    .bt_status_y               = 22,
    .bt_status_font            = &lv_font_montserrat_12_pl,
    .bt_slider_x               = 14,
    .bt_slider_y               = 40,
    .bt_slider_w               = 100,
    .bt_slider_h               = 4,
    .bt_vol_label_x            = 46,
    .bt_vol_label_y            = 50,
    .bt_vol_label_font         = &lv_font_montserrat_12_pl,
    .bt_show_mode_indicator    = false,
    .bt_show_clock             = false,
    .bt_mode_indic_x           = 108,
    .bt_mode_indic_y           = 8,
    .bt_clock_widget_x         = 39,
    .bt_clock_widget_y         = 0,
    .bt_title_x                = 4,
    .bt_title_y                = 30,
    .bt_title_w                = 124,
    .bt_title_font             = &lv_font_montserrat_12_pl,
    .bt_artist_x               = 4,
    .bt_artist_y               = 42,
    .bt_artist_w               = 124,
    .bt_artist_font            = &lv_font_montserrat_12_pl,
    .bt_time_x                 = 80,
    .bt_time_y                 = 54,
    .bt_time_font              = &lv_font_montserrat_12_pl,

    .settings_title_y          = 2,
    .settings_row_w            = 124,
    .settings_row_h            = 22,
    .settings_row1_y           = 16,
    .settings_row2_y           = 40,
    .settings_row3_y           = 64,
    .settings_slider_w         = 80,
    .settings_slider_h         = 3,
    .settings_hint_y           = -2,
    .settings_title_font       = &lv_font_montserrat_12_pl,
    .settings_row_font         = &lv_font_montserrat_12_pl,
    .settings_value_font       = &lv_font_montserrat_12_pl,
    .settings_hint_font        = &lv_font_montserrat_12_pl,

    // 128x64 mono — 10 bands very tight: 12 px/band wide, thin slider
    .eq_title_y                = 0,
    .eq_info_y                 = 0,
    .eq_band_area_y            = 12,
    .eq_slider_h               = 38,
    .eq_slider_w               = 3,
    .eq_band_w                 = 12,
    .eq_hint_y                 = -2,
    .eq_title_font             = &lv_font_montserrat_12_pl,
    .eq_info_font              = &lv_font_montserrat_12_pl,
    .eq_freq_font              = &lv_font_montserrat_12_pl,
    .eq_hint_font              = &lv_font_montserrat_12_pl,

    .events_header_h           = 12,
    .events_item_h             = 22,
    .events_item_pad           = 1,
    .events_row_w              = 124,
    .events_row_label_w        = 116,
    .events_row_pad_hor        = 4,
    .events_header_font        = &lv_font_montserrat_12_pl,
    .events_title_font         = &lv_font_montserrat_12_pl,
    .events_meta_font          = &lv_font_montserrat_12_pl,

    .wifi_title_y              = 2,
    .wifi_card_w               = 124,
    .wifi_card_h               = 44,
    .wifi_card_y               = 2,
    .wifi_card_pad_hor         = 6,
    .wifi_card_pad_ver         = 4,
    .wifi_row2_y               = 12,
    .wifi_row3_y               = 24,
    .wifi_hint_y               = -2,
    .wifi_title_font           = &lv_font_montserrat_12_pl,
    .wifi_key_font             = &lv_font_montserrat_12_pl,
    .wifi_value_font           = &lv_font_montserrat_12_pl,
    .wifi_hint_font            = &lv_font_montserrat_12_pl,
};

#elif defined(UI_PROFILE_240X296)

static const ui_profile_t k_defaults = {
    // Clock Panel: Large clock on top, information bar underneath
    .clock_panel_x             = 0,
    .clock_panel_y             = 0,
    .clock_panel_w             = 240,
    .clock_panel_h             = 188,
    .clock_strip_x             = 0,
    .clock_strip_y             = 193,
    .clock_strip_w             = 240,
    .clock_strip_h             = 103,
    .clock_strip_label_w       = 220,
    .clock_strip_station_y     = 10,
    .clock_strip_title_y       = 40,
    .clock_strip_station_font  = &lv_font_montserrat_14_pl,
    .clock_strip_title_font    = &lv_font_montserrat_12_pl,
    .clock_time_x              = 27,  // Centering depends on the font, here a slight offset
    .clock_time_y              = 45,
    .clock_time_font           = &lv_font_montserrat_72, // Choose the largest available, e.g. 48 or 64
    .clock_show_time           = true,
    .clock_date_x              = 49,
    .clock_date_y              = 128,
    .clock_date_font           = &lv_font_montserrat_18_pl,
    .clock_show_date           = true,
    .clock_show_strip          = true,
    .clock_show_mode_indicator = true,
    .clock_show_event_indicator = true,
    .clock_mode_indic_x        = 170,
    .clock_mode_indic_y        = 1,
    .clock_event_indic_x       = 150,
    .clock_event_indic_y       = 1,

    // Radio: Station info and large volume slider
    .radio_np_x                = 10,
    .radio_np_y                = 35,
    .radio_show_np             = true,
    .radio_state_x             = 92,
    .radio_state_y             = 100,
    .radio_state_font          = &lv_font_montserrat_14_pl,
    .radio_audio_info_x        = 61,
    .radio_audio_info_y        = 162,
    .radio_audio_info_font     = &lv_font_montserrat_12_pl,
    .radio_slider_x            = 30,
    .radio_slider_y            = 224,
    .radio_slider_w            = 180,
    .radio_slider_h            = 12,
    .radio_vol_label_x         = 102,
    .radio_vol_label_y         = 247,
    .radio_vol_label_font      = &lv_font_montserrat_14_pl,
    .radio_show_mode_indicator = true,
    .radio_show_clock          = true,
    .radio_mode_indic_x        = 158,
    .radio_mode_indic_y        = 1,
    .radio_clock_widget_x      = 95,
    .radio_clock_widget_y      = 0,

    // Playlist: We use the screen height for more list items
    .playlist_header_h         = 45,
    .playlist_item_h           = 34,
    .playlist_item_pad         = 2,
    .playlist_row_w            = 234,
    .playlist_row_label_w      = 210,
    .playlist_row_pad_left     = 10,
    .playlist_label_x          = 90,
    .playlist_label_y          = -10,
    .playlist_hint_x           = -20,
    .playlist_hint_y           = 10,
    .playlist_header_font      = &lv_font_montserrat_14_pl,
    .playlist_row_font         = &lv_font_montserrat_14_pl,

    // Bluetooth: small BT icon at top, brand + status, then track metadata
    .bt_circle_x               = 90,
    .bt_circle_y               = 35,
    .bt_circle_w               = 60,
    .bt_circle_h               = 60,
    .bt_icon_font              = &lv_font_montserrat_18_pl,
    .bt_show_circle            = true,
    .bt_brand_x                = 60,
    .bt_brand_y                = 100,
    .bt_brand_font             = &lv_font_montserrat_14_pl,
    .bt_status_x               = 80,
    .bt_status_y               = 122,
    .bt_status_font            = &lv_font_montserrat_12_pl,
    .bt_slider_x               = 30,
    .bt_slider_y               = 230,
    .bt_slider_w               = 180,
    .bt_slider_h               = 10,
    .bt_vol_label_x            = 95,
    .bt_vol_label_y            = 255,
    .bt_vol_label_font         = &lv_font_montserrat_14_pl,
    .bt_show_mode_indicator    = true,
    .bt_show_clock             = true,
    .bt_mode_indic_x           = 158,
    .bt_mode_indic_y           = 1,
    .bt_clock_widget_x         = 95,
    .bt_clock_widget_y         = 0,
    .bt_title_x                = 10,
    .bt_title_y                = 150,
    .bt_title_w                = 220,
    .bt_title_font             = &lv_font_montserrat_14_pl,
    .bt_artist_x               = 10,
    .bt_artist_y               = 180,
    .bt_artist_w               = 220,
    .bt_artist_font            = &lv_font_montserrat_12_pl,
    .bt_time_x                 = 90,
    .bt_time_y                 = 240,
    .bt_time_font              = &lv_font_montserrat_14_pl,

    // Settings
    .settings_title_y          = 10,
    .settings_row_w            = 220,
    .settings_row_h            = 60,
    .settings_row1_y           = 45,
    .settings_row2_y           = 115,
    .settings_row3_y           = 185,
    .settings_slider_w         = 140,
    .settings_slider_h         = 8,
    .settings_hint_y           = -10,
    .settings_title_font       = &lv_font_montserrat_14_pl,
    .settings_row_font         = &lv_font_montserrat_14_pl,
    .settings_value_font       = &lv_font_montserrat_14_pl,
    .settings_hint_font        = &lv_font_montserrat_12_pl,

    // Equalizer: Narrower bars to fit 10 bands in a 240 width
    .eq_title_y                = 10,
    .eq_info_y                 = 35,
    .eq_band_area_y            = 70,
    .eq_slider_h               = 160,
    .eq_slider_w               = 6,
    .eq_band_w                 = 22, // 240 / 10 bands = 24 max
    .eq_hint_y                 = -8,
    .eq_title_font             = &lv_font_montserrat_18_pl,
    .eq_info_font              = &lv_font_montserrat_14_pl,
    .eq_freq_font              = &lv_font_montserrat_12_pl, // Smaller font for frequency
    .eq_hint_font              = &lv_font_montserrat_12_pl,

    // Events
    .events_header_h           = 30,
    .events_item_h             = 50,
    .events_item_pad           = 4,
    .events_row_w              = 232,
    .events_row_label_w        = 215,
    .events_row_pad_hor        = 8,
    .events_header_font        = &lv_font_montserrat_14_pl,
    .events_title_font         = &lv_font_montserrat_14_pl,
    .events_meta_font          = &lv_font_montserrat_12_pl,

    // WiFi Card
    .wifi_title_y              = 20,
    .wifi_card_w               = 220,
    .wifi_card_h               = 160,
    .wifi_card_y               = 10,
    .wifi_card_pad_hor         = 12,
    .wifi_card_pad_ver         = 15,
    .wifi_row2_y               = 45,
    .wifi_row3_y               = 90,
    .wifi_hint_y               = -15,
    .wifi_title_font           = &lv_font_montserrat_18_pl,
    .wifi_key_font             = &lv_font_montserrat_12_pl,
    .wifi_value_font           = &lv_font_montserrat_14_pl,
    .wifi_hint_font            = &lv_font_montserrat_12_pl,
};

#elif defined(UI_PROFILE_320x240)

static const ui_profile_t k_defaults = {
    .clock_panel_x             = 0,
    .clock_panel_y             = 0,
    .clock_panel_w             = 320,
    .clock_panel_h             = 175,
    .clock_strip_x             = 0,
    .clock_strip_y             = 178,
    .clock_strip_w             = 320,
    .clock_strip_h             = 62,
    .clock_strip_label_w       = 296,
    .clock_strip_station_y     = 8,
    .clock_strip_title_y       = 32,
    .clock_strip_station_font  = &lv_font_montserrat_14_pl,
    .clock_strip_title_font    = &lv_font_montserrat_12_pl,
    .clock_time_x              = 40,
    .clock_time_y              = 25,
    .clock_time_font           = &lv_font_montserrat_96,
    .clock_show_time           = true,
    .clock_date_x              = 80,
    .clock_date_y              = 130,
    .clock_date_font           = &lv_font_montserrat_18_pl,
    .clock_show_date           = true,
    .clock_show_strip          = true,
    .clock_show_mode_indicator = true,
    .clock_show_event_indicator = true,
    .clock_mode_indic_x        = 300,
    .clock_mode_indic_y        = 8,
    .clock_event_indic_x       = 282,
    .clock_event_indic_y       = 8,

    .radio_np_x                = 10,
    .radio_np_y                = 35,
    .radio_show_np             = true,
    .radio_state_x             = 132,
    .radio_state_y             = 62,
    .radio_state_font          = &lv_font_montserrat_14_pl,
    .radio_audio_info_x        = 100,
    .radio_audio_info_y        = 124,
    .radio_audio_info_font     = &lv_font_montserrat_12_pl,
    .radio_slider_x            = 40,
    .radio_slider_y            = 192,
    .radio_slider_w            = 240,
    .radio_slider_h            = 8,
    .radio_vol_label_x         = 149,
    .radio_vol_label_y         = 208,
    .radio_vol_label_font      = &lv_font_montserrat_12_pl,
    .radio_show_mode_indicator = true,
    .radio_show_clock          = true,
    .radio_mode_indic_x        = 300,
    .radio_mode_indic_y        = 8,
    .radio_clock_widget_x      = 135,
    .radio_clock_widget_y      = 0,

    .playlist_header_h         = 26,
    .playlist_item_h           = 28,
    .playlist_item_pad         = 2,
    .playlist_row_w            = 314,
    .playlist_row_label_w      = 298,
    .playlist_row_pad_left     = 8,
    .playlist_header_font      = &lv_font_montserrat_14_pl,
    .playlist_row_font         = &lv_font_montserrat_12_pl,

    // 320x240 — small BT icon top, brand + status, then track metadata
    .bt_circle_x               = 130,
    .bt_circle_y               = 25,
    .bt_circle_w               = 60,
    .bt_circle_h               = 60,
    .bt_icon_font              = &lv_font_montserrat_18_pl,
    .bt_show_circle            = true,
    .bt_brand_x                = 110,
    .bt_brand_y                = 90,
    .bt_brand_font             = &lv_font_montserrat_14_pl,
    .bt_status_x               = 135,
    .bt_status_y               = 112,
    .bt_status_font            = &lv_font_montserrat_12_pl,
    .bt_slider_x               = 70,
    .bt_slider_y               = 195,
    .bt_slider_w               = 180,
    .bt_slider_h               = 10,
    .bt_vol_label_x            = 135,
    .bt_vol_label_y            = 213,
    .bt_vol_label_font         = &lv_font_montserrat_12_pl,
    .bt_show_mode_indicator    = true,
    .bt_show_clock             = true,
    .bt_mode_indic_x           = 300,
    .bt_mode_indic_y           = 8,
    .bt_clock_widget_x         = 135,
    .bt_clock_widget_y         = 0,
    .bt_title_x                = 10,
    .bt_title_y                = 140,
    .bt_title_w                = 300,
    .bt_title_font             = &lv_font_montserrat_14_pl,
    .bt_artist_x               = 10,
    .bt_artist_y               = 165,
    .bt_artist_w               = 300,
    .bt_artist_font            = &lv_font_montserrat_12_pl,
    .bt_time_x                 = 135,
    .bt_time_y                 = 213,
    .bt_time_font              = &lv_font_montserrat_14_pl,

    .settings_title_y          = 6,
    .settings_row_w            = 288,
    .settings_row_h            = 68,
    .settings_row1_y           = 32,
    .settings_row2_y           = 112,
    .settings_row3_y           = 192,
    .settings_slider_w         = 170,
    .settings_slider_h         = 6,
    .settings_hint_y           = -6,
    .settings_title_font       = &lv_font_montserrat_14_pl,
    .settings_row_font         = &lv_font_montserrat_14_pl,
    .settings_value_font       = &lv_font_montserrat_14_pl,
    .settings_hint_font        = &lv_font_montserrat_12_pl,

    .eq_title_y                = 6,
    .eq_info_y                 = 30,
    .eq_band_area_y            = 62,
    .eq_slider_h               = 132,
    .eq_slider_w               = 8,
    .eq_band_w                 = 28,
    .eq_hint_y                 = -6,
    .eq_title_font             = &lv_font_montserrat_18_pl,
    .eq_info_font              = &lv_font_montserrat_14_pl,
    .eq_freq_font              = &lv_font_montserrat_12_pl,
    .eq_hint_font              = &lv_font_montserrat_12_pl,

    .events_header_h           = 26,
    .events_item_h             = 44,
    .events_item_pad           = 3,
    .events_row_w              = 314,
    .events_row_label_w        = 298,
    .events_row_pad_hor        = 8,
    .events_header_font        = &lv_font_montserrat_14_pl,
    .events_title_font         = &lv_font_montserrat_14_pl,
    .events_meta_font          = &lv_font_montserrat_12_pl,

    .wifi_title_y              = 14,
    .wifi_card_w               = 290,
    .wifi_card_h               = 136,
    .wifi_card_y               = 6,
    .wifi_card_pad_hor         = 16,
    .wifi_card_pad_ver         = 12,
    .wifi_row2_y               = 36,
    .wifi_row3_y               = 72,
    .wifi_hint_y               = -10,
    .wifi_title_font           = &lv_font_montserrat_18_pl,
    .wifi_key_font             = &lv_font_montserrat_12_pl,
    .wifi_value_font           = &lv_font_montserrat_18_pl,
    .wifi_hint_font            = &lv_font_montserrat_12_pl,
};

#else
    #error "Unknown UI_PROFILE"
#endif

// Mutable runtime — copy of defaults at startup, overwritten from JSON / API.
// Can't initialize as a copy of `k_defaults` (that's runtime-const, not
// compile-time-const), so we zero-init and copy in ui_profile_load_from_file().
// To make pre-load access (before JSON load) still see defaults, we also copy
// in ui_profile_get() on first call.
static ui_profile_t s_runtime;
static bool         s_initialized = false;

static void ensure_initialized(void)
{
    if (!s_initialized) {
        s_runtime = k_defaults;
        s_initialized = true;
    }
}

const ui_profile_t *ui_profile_get(void)
{
    ensure_initialized();
    return &s_runtime;
}

const ui_profile_t *ui_profile_defaults(void) { return &k_defaults; }

void ui_profile_set(const ui_profile_t *src)
{
    if (!src) return;
    ensure_initialized();
    s_runtime = *src;
}

void ui_profile_reset(void)
{
    s_runtime = k_defaults;
    s_initialized = true;
}

// ── JSON helpers ────────────────────────────────────────────────────────────

static void load_i16(const cJSON *obj, const char *key, int16_t *dst)
{
    const cJSON *it = cJSON_GetObjectItem(obj, key);
    if (cJSON_IsNumber(it)) *dst = (int16_t)it->valueint;
}

static void load_bool(const cJSON *obj, const char *key, bool *dst)
{
    const cJSON *it = cJSON_GetObjectItem(obj, key);
    if (cJSON_IsBool(it)) *dst = cJSON_IsTrue(it);
}

static void load_font(const cJSON *obj, const char *key, const lv_font_t **dst)
{
    const cJSON *it = cJSON_GetObjectItem(obj, key);
    if (!cJSON_IsString(it)) return;
    const lv_font_t *f = ui_font_by_id(it->valuestring);
    if (f) *dst = f;
}

static void add_i16(cJSON *obj, const char *key, int16_t v)
{
    cJSON_AddNumberToObject(obj, key, v);
}

static void add_bool(cJSON *obj, const char *key, bool v)
{
    cJSON_AddBoolToObject(obj, key, v);
}

static void add_font(cJSON *obj, const char *key, const lv_font_t *f)
{
    cJSON_AddStringToObject(obj, key, ui_font_id(f));
}

// ── per-section load ────────────────────────────────────────────────────────

static void load_clock(const cJSON *obj, ui_profile_t *p)
{
    if (!cJSON_IsObject(obj)) return;
    load_i16 (obj, "clock_panel_x",              &p->clock_panel_x);
    load_i16 (obj, "clock_panel_y",              &p->clock_panel_y);
    load_i16 (obj, "clock_panel_w",              &p->clock_panel_w);
    load_i16 (obj, "clock_panel_h",              &p->clock_panel_h);
    load_i16 (obj, "clock_strip_x",              &p->clock_strip_x);
    load_i16 (obj, "clock_strip_y",              &p->clock_strip_y);
    load_i16 (obj, "clock_strip_w",              &p->clock_strip_w);
    load_i16 (obj, "clock_strip_h",              &p->clock_strip_h);
    load_i16 (obj, "clock_strip_label_w",        &p->clock_strip_label_w);
    load_i16 (obj, "clock_strip_station_y",      &p->clock_strip_station_y);
    load_i16 (obj, "clock_strip_title_y",        &p->clock_strip_title_y);
    load_font(obj, "clock_strip_station_font",   &p->clock_strip_station_font);
    load_font(obj, "clock_strip_title_font",     &p->clock_strip_title_font);
    load_i16 (obj, "clock_time_x",               &p->clock_time_x);
    load_i16 (obj, "clock_time_y",               &p->clock_time_y);
    load_font(obj, "clock_time_font",            &p->clock_time_font);
    load_bool(obj, "clock_show_time",            &p->clock_show_time);
    load_i16 (obj, "clock_date_x",               &p->clock_date_x);
    load_i16 (obj, "clock_date_y",               &p->clock_date_y);
    load_font(obj, "clock_date_font",            &p->clock_date_font);
    load_bool(obj, "clock_show_date",            &p->clock_show_date);
    load_bool(obj, "clock_show_strip",           &p->clock_show_strip);
    load_bool(obj, "clock_show_mode_indicator",  &p->clock_show_mode_indicator);
    load_bool(obj, "clock_show_event_indicator", &p->clock_show_event_indicator);
    load_i16 (obj, "clock_mode_indic_x",         &p->clock_mode_indic_x);
    load_i16 (obj, "clock_mode_indic_y",         &p->clock_mode_indic_y);
    load_i16 (obj, "clock_event_indic_x",        &p->clock_event_indic_x);
    load_i16 (obj, "clock_event_indic_y",        &p->clock_event_indic_y);
}

static void load_bt(const cJSON *obj, ui_profile_t *p)
{
    if (!cJSON_IsObject(obj)) return;
    load_i16 (obj, "bt_circle_x",            &p->bt_circle_x);
    load_i16 (obj, "bt_circle_y",            &p->bt_circle_y);
    load_i16 (obj, "bt_circle_w",            &p->bt_circle_w);
    load_i16 (obj, "bt_circle_h",            &p->bt_circle_h);
    load_font(obj, "bt_icon_font",           &p->bt_icon_font);
    load_bool(obj, "bt_show_circle",         &p->bt_show_circle);
    load_i16 (obj, "bt_brand_x",             &p->bt_brand_x);
    load_i16 (obj, "bt_brand_y",             &p->bt_brand_y);
    load_font(obj, "bt_brand_font",          &p->bt_brand_font);
    load_i16 (obj, "bt_status_x",            &p->bt_status_x);
    load_i16 (obj, "bt_status_y",            &p->bt_status_y);
    load_font(obj, "bt_status_font",         &p->bt_status_font);
    load_i16 (obj, "bt_slider_x",            &p->bt_slider_x);
    load_i16 (obj, "bt_slider_y",            &p->bt_slider_y);
    load_i16 (obj, "bt_slider_w",            &p->bt_slider_w);
    load_i16 (obj, "bt_slider_h",            &p->bt_slider_h);
    load_i16 (obj, "bt_vol_label_x",         &p->bt_vol_label_x);
    load_i16 (obj, "bt_vol_label_y",         &p->bt_vol_label_y);
    load_font(obj, "bt_vol_label_font",      &p->bt_vol_label_font);
    load_bool(obj, "bt_show_mode_indicator", &p->bt_show_mode_indicator);
    load_bool(obj, "bt_show_clock",          &p->bt_show_clock);
    load_i16 (obj, "bt_mode_indic_x",        &p->bt_mode_indic_x);
    load_i16 (obj, "bt_mode_indic_y",        &p->bt_mode_indic_y);
    load_i16 (obj, "bt_clock_widget_x",      &p->bt_clock_widget_x);
    load_i16 (obj, "bt_clock_widget_y",      &p->bt_clock_widget_y);
    load_i16 (obj, "bt_title_x",             &p->bt_title_x);
    load_i16 (obj, "bt_title_y",             &p->bt_title_y);
    load_i16 (obj, "bt_title_w",             &p->bt_title_w);
    load_font(obj, "bt_title_font",          &p->bt_title_font);
    load_i16 (obj, "bt_artist_x",            &p->bt_artist_x);
    load_i16 (obj, "bt_artist_y",            &p->bt_artist_y);
    load_i16 (obj, "bt_artist_w",            &p->bt_artist_w);
    load_font(obj, "bt_artist_font",         &p->bt_artist_font);
    load_i16 (obj, "bt_time_x",              &p->bt_time_x);
    load_i16 (obj, "bt_time_y",              &p->bt_time_y);
    load_font(obj, "bt_time_font",           &p->bt_time_font);
}

static void load_radio(const cJSON *obj, ui_profile_t *p)
{
    if (!cJSON_IsObject(obj)) return;
    load_i16 (obj, "radio_np_x",                &p->radio_np_x);
    load_i16 (obj, "radio_np_y",                &p->radio_np_y);
    load_bool(obj, "radio_show_np",             &p->radio_show_np);
    load_i16 (obj, "radio_state_x",             &p->radio_state_x);
    load_i16 (obj, "radio_state_y",             &p->radio_state_y);
    load_font(obj, "radio_state_font",          &p->radio_state_font);
    load_i16 (obj, "radio_audio_info_x",        &p->radio_audio_info_x);
    load_i16 (obj, "radio_audio_info_y",        &p->radio_audio_info_y);
    load_font(obj, "radio_audio_info_font",     &p->radio_audio_info_font);
    load_i16 (obj, "radio_slider_x",            &p->radio_slider_x);
    load_i16 (obj, "radio_slider_y",            &p->radio_slider_y);
    load_i16 (obj, "radio_slider_w",            &p->radio_slider_w);
    load_i16 (obj, "radio_slider_h",            &p->radio_slider_h);
    load_i16 (obj, "radio_vol_label_x",         &p->radio_vol_label_x);
    load_i16 (obj, "radio_vol_label_y",         &p->radio_vol_label_y);
    load_font(obj, "radio_vol_label_font",      &p->radio_vol_label_font);
    load_bool(obj, "radio_show_mode_indicator", &p->radio_show_mode_indicator);
    load_bool(obj, "radio_show_clock",          &p->radio_show_clock);
    load_i16 (obj, "radio_mode_indic_x",        &p->radio_mode_indic_x);
    load_i16 (obj, "radio_mode_indic_y",        &p->radio_mode_indic_y);
    load_i16 (obj, "radio_clock_widget_x",      &p->radio_clock_widget_x);
    load_i16 (obj, "radio_clock_widget_y",      &p->radio_clock_widget_y);
}

static cJSON *dump_radio(const ui_profile_t *p)
{
    cJSON *o = cJSON_CreateObject();
    add_i16 (o, "radio_np_x",                p->radio_np_x);
    add_i16 (o, "radio_np_y",                p->radio_np_y);
    add_bool(o, "radio_show_np",             p->radio_show_np);
    add_i16 (o, "radio_state_x",             p->radio_state_x);
    add_i16 (o, "radio_state_y",             p->radio_state_y);
    add_font(o, "radio_state_font",          p->radio_state_font);
    add_i16 (o, "radio_audio_info_x",        p->radio_audio_info_x);
    add_i16 (o, "radio_audio_info_y",        p->radio_audio_info_y);
    add_font(o, "radio_audio_info_font",     p->radio_audio_info_font);
    add_i16 (o, "radio_slider_x",            p->radio_slider_x);
    add_i16 (o, "radio_slider_y",            p->radio_slider_y);
    add_i16 (o, "radio_slider_w",            p->radio_slider_w);
    add_i16 (o, "radio_slider_h",            p->radio_slider_h);
    add_i16 (o, "radio_vol_label_x",         p->radio_vol_label_x);
    add_i16 (o, "radio_vol_label_y",         p->radio_vol_label_y);
    add_font(o, "radio_vol_label_font",      p->radio_vol_label_font);
    add_bool(o, "radio_show_mode_indicator", p->radio_show_mode_indicator);
    add_bool(o, "radio_show_clock",          p->radio_show_clock);
    add_i16 (o, "radio_mode_indic_x",        p->radio_mode_indic_x);
    add_i16 (o, "radio_mode_indic_y",        p->radio_mode_indic_y);
    add_i16 (o, "radio_clock_widget_x",      p->radio_clock_widget_x);
    add_i16 (o, "radio_clock_widget_y",      p->radio_clock_widget_y);
    return o;
}

static cJSON *dump_bt(const ui_profile_t *p)
{
    cJSON *o = cJSON_CreateObject();
    add_i16 (o, "bt_circle_x",            p->bt_circle_x);
    add_i16 (o, "bt_circle_y",            p->bt_circle_y);
    add_i16 (o, "bt_circle_w",            p->bt_circle_w);
    add_i16 (o, "bt_circle_h",            p->bt_circle_h);
    add_font(o, "bt_icon_font",           p->bt_icon_font);
    add_bool(o, "bt_show_circle",         p->bt_show_circle);
    add_i16 (o, "bt_brand_x",             p->bt_brand_x);
    add_i16 (o, "bt_brand_y",             p->bt_brand_y);
    add_font(o, "bt_brand_font",          p->bt_brand_font);
    add_i16 (o, "bt_status_x",            p->bt_status_x);
    add_i16 (o, "bt_status_y",            p->bt_status_y);
    add_font(o, "bt_status_font",         p->bt_status_font);
    add_i16 (o, "bt_slider_x",            p->bt_slider_x);
    add_i16 (o, "bt_slider_y",            p->bt_slider_y);
    add_i16 (o, "bt_slider_w",            p->bt_slider_w);
    add_i16 (o, "bt_slider_h",            p->bt_slider_h);
    add_i16 (o, "bt_vol_label_x",         p->bt_vol_label_x);
    add_i16 (o, "bt_vol_label_y",         p->bt_vol_label_y);
    add_font(o, "bt_vol_label_font",      p->bt_vol_label_font);
    add_bool(o, "bt_show_mode_indicator", p->bt_show_mode_indicator);
    add_bool(o, "bt_show_clock",          p->bt_show_clock);
    add_i16 (o, "bt_mode_indic_x",        p->bt_mode_indic_x);
    add_i16 (o, "bt_mode_indic_y",        p->bt_mode_indic_y);
    add_i16 (o, "bt_clock_widget_x",      p->bt_clock_widget_x);
    add_i16 (o, "bt_clock_widget_y",      p->bt_clock_widget_y);
    add_i16 (o, "bt_title_x",             p->bt_title_x);
    add_i16 (o, "bt_title_y",             p->bt_title_y);
    add_i16 (o, "bt_title_w",             p->bt_title_w);
    add_font(o, "bt_title_font",          p->bt_title_font);
    add_i16 (o, "bt_artist_x",            p->bt_artist_x);
    add_i16 (o, "bt_artist_y",            p->bt_artist_y);
    add_i16 (o, "bt_artist_w",            p->bt_artist_w);
    add_font(o, "bt_artist_font",         p->bt_artist_font);
    add_i16 (o, "bt_time_x",              p->bt_time_x);
    add_i16 (o, "bt_time_y",              p->bt_time_y);
    add_font(o, "bt_time_font",           p->bt_time_font);
    return o;
}

static cJSON *dump_clock(const ui_profile_t *p)
{
    cJSON *o = cJSON_CreateObject();
    add_i16 (o, "clock_panel_x",              p->clock_panel_x);
    add_i16 (o, "clock_panel_y",              p->clock_panel_y);
    add_i16 (o, "clock_panel_w",              p->clock_panel_w);
    add_i16 (o, "clock_panel_h",              p->clock_panel_h);
    add_i16 (o, "clock_strip_x",              p->clock_strip_x);
    add_i16 (o, "clock_strip_y",              p->clock_strip_y);
    add_i16 (o, "clock_strip_w",              p->clock_strip_w);
    add_i16 (o, "clock_strip_h",              p->clock_strip_h);
    add_i16 (o, "clock_strip_label_w",        p->clock_strip_label_w);
    add_i16 (o, "clock_strip_station_y",      p->clock_strip_station_y);
    add_i16 (o, "clock_strip_title_y",        p->clock_strip_title_y);
    add_font(o, "clock_strip_station_font",   p->clock_strip_station_font);
    add_font(o, "clock_strip_title_font",     p->clock_strip_title_font);
    add_i16 (o, "clock_time_x",               p->clock_time_x);
    add_i16 (o, "clock_time_y",               p->clock_time_y);
    add_font(o, "clock_time_font",            p->clock_time_font);
    add_bool(o, "clock_show_time",            p->clock_show_time);
    add_i16 (o, "clock_date_x",               p->clock_date_x);
    add_i16 (o, "clock_date_y",               p->clock_date_y);
    add_font(o, "clock_date_font",            p->clock_date_font);
    add_bool(o, "clock_show_date",            p->clock_show_date);
    add_bool(o, "clock_show_strip",           p->clock_show_strip);
    add_bool(o, "clock_show_mode_indicator",  p->clock_show_mode_indicator);
    add_bool(o, "clock_show_event_indicator", p->clock_show_event_indicator);
    add_i16 (o, "clock_mode_indic_x",         p->clock_mode_indic_x);
    add_i16 (o, "clock_mode_indic_y",         p->clock_mode_indic_y);
    add_i16 (o, "clock_event_indic_x",        p->clock_event_indic_x);
    add_i16 (o, "clock_event_indic_y",        p->clock_event_indic_y);
    return o;
}

// ── I/O ─────────────────────────────────────────────────────────────────────

esp_err_t ui_profile_load_from_file(void)
{
    // Defaults first — missing JSON fields won't leave zeros.
    ui_profile_reset();

    FILE *f = fopen(UI_PROFILE_FILE, "r");
    if (!f) {
        ESP_LOGI(TAG, "%s missing — using defaults", UI_PROFILE_FILE);
        return ESP_ERR_NOT_FOUND;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    if (size <= 0 || size > 16384) {
        fclose(f);
        ESP_LOGW(TAG, "%s invalid size: %ld", UI_PROFILE_FILE, size);
        return ESP_FAIL;
    }

    char *buf = malloc(size + 1);
    if (!buf) {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }
    fread(buf, 1, size, f);
    buf[size] = 0;
    fclose(f);

    cJSON *json = cJSON_Parse(buf);
    free(buf);
    if (!json) {
        ESP_LOGW(TAG, "%s parse error — using defaults", UI_PROFILE_FILE);
        return ESP_FAIL;
    }

    load_clock(cJSON_GetObjectItem(json, "clock"), &s_runtime);
    load_bt   (cJSON_GetObjectItem(json, "bt"),    &s_runtime);
    load_radio(cJSON_GetObjectItem(json, "radio"), &s_runtime);
    // (other sections will land here when exposed in the UI: playlist, ...)

    cJSON_Delete(json);
    ESP_LOGI(TAG, "%s loaded", UI_PROFILE_FILE);
    return ESP_OK;
}

// ── public per-section helpers ──────────────────────────────────────────────

void *ui_profile_dump_clock(void)
{
    ensure_initialized();
    return dump_clock(&s_runtime);
}

void ui_profile_patch_clock(const void *obj)
{
    ensure_initialized();
    load_clock((const cJSON *)obj, &s_runtime);
}

void *ui_profile_dump_bt(void)
{
    ensure_initialized();
    return dump_bt(&s_runtime);
}

void ui_profile_patch_bt(const void *obj)
{
    ensure_initialized();
    load_bt((const cJSON *)obj, &s_runtime);
}

void *ui_profile_dump_radio(void)
{
    ensure_initialized();
    return dump_radio(&s_runtime);
}

void ui_profile_patch_radio(const void *obj)
{
    ensure_initialized();
    load_radio((const cJSON *)obj, &s_runtime);
}

esp_err_t ui_profile_save_to_file(void)
{
    ensure_initialized();

    cJSON *json = cJSON_CreateObject();
    cJSON_AddItemToObject(json, "clock", dump_clock(&s_runtime));
    cJSON_AddItemToObject(json, "bt",    dump_bt   (&s_runtime));
    cJSON_AddItemToObject(json, "radio", dump_radio(&s_runtime));

    char *str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    if (!str) return ESP_ERR_NO_MEM;

    FILE *f = fopen(UI_PROFILE_FILE, "w");
    if (!f) {
        free(str);
        return ESP_FAIL;
    }
    fwrite(str, 1, strlen(str), f);
    fclose(f);
    free(str);

    ESP_LOGI(TAG, "%s saved", UI_PROFILE_FILE);
    return ESP_OK;
}
