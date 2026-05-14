#pragma once

#include "radio_service.h"
#include "ui_events.h"
#include <stdbool.h>

#define APP_STATE_MAX_SUBSCRIBERS 4

// Defined here — no LVGL dependency, imported by both ui and settings_ex
typedef enum
{
    THEME_DARK = 0,
    THEME_LIGHT = 1
} ui_theme_t;

typedef enum
{
    BT_CONNECTED = 0,
    BT_DISCONNECTED = 1,
    BT_DISCOVERABLE
} bt_state_t;

typedef struct {
    radio_state_t radio_state;
    int volume;                 // Radio volume
    int eq[10];                 // Equalizer values
    bool eq_enabled;            // EQ on/off (volume works independently)
    char url[256];              // URL radio station
    char station_name[64];      // Station name
    char title[128];            // Music title
    int curr_index;             // Current station index in playlist
    int sample_rate;            // Audio sample rate
    int bits;                   // Audio bits
    int channels;               // Num od audio channels
    int bitrate;                // Bitrate
    int codec_fmt;              // Audio codec
    bool bt_enable;             // Bluetooth enable
    bool bt_show_screen;        // Bluetooth show/hide screen
    bt_state_t bt_state;        // Bluetooth connected
    int bt_volume;              // Bluetooth volume
    bool time_synced;           // Time synced
    ui_screen_id_t screen;      // Current screen
    int brightness;             // Display brightness
    ui_theme_t theme;
    int  scrsaver_delay;        // Idle seconds before activation (0 = off)
    int  scrsaver_id;           // screensaver_id_t (see screensavers.h)
} app_state_t;

typedef struct {
    bool has_radio;
    radio_state_t radio_state;

    bool has_volume;
    int volume;

    bool has_eq;
    int eq[10];

    bool has_eq_enabled;
    bool eq_enabled;

    bool has_url;
    const char *url;

    bool has_station_name;
    const char *station_name;

    bool has_title;
    const char *title;

    bool has_curr_index;
    int curr_index;

    bool has_audio_info;
    int sample_rate;
    int bits;
    int channels;
    int bitrate;
    int codec_fmt;

    bool has_bt_enable;
    bool bt_enable;
    bool has_bt_show_screen;
    bool bt_show_screen;
    bool has_bt_state;
    ui_theme_t bt_state;
    bool has_bt_volume;
    int bt_volume;

    bool has_time_synced;
    bool time_synced;

    bool has_screen;
    ui_screen_id_t screen;
    
    bool has_display_brightness;
    int display_brightness;

    bool has_theme;
    ui_theme_t theme;

    bool has_scrsaver_delay;
    int  scrsaver_delay;
    bool has_scrsaver_id;
    int  scrsaver_id;
} app_state_patch_t;

typedef void (*app_state_cb_t)(void);

void app_state_init(void);
app_state_t* app_state_get(void);
void app_state_subscribe(app_state_cb_t cb);
void app_state_update(const app_state_patch_t *patch);