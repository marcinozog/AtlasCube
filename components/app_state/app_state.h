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
    bool bt_auto_switch;        // Exclusive source: BT play stops radio & vice versa
    bt_state_t bt_state;        // Bluetooth connected
    bool bt_playing;            // Phone AVRCP playback active (true on play, false on pause/stop)
    int bt_volume;              // Bluetooth volume
    bool bt_vol_sync;           // Sync module volume with the phone
    char bt_title[128];         // BT track title
    char bt_artist[64];         // BT track artist
    int  bt_duration_ms;        // BT track length (ms)
    int  bt_position_s;         // BT current position (s)
    char bt_codec[16];          // BT audio codec name (e.g. "LDAC")
    int  bt_sample_rate;        // BT audio sample rate (Hz)
    int  bt_bits;               // BT audio bit depth
    bool time_synced;           // Time synced
    ui_screen_id_t screen;      // Current screen
    int brightness;             // Display brightness
    ui_theme_t theme;
    bool bg_gradient;           // Dithered gradient background on/off (false = solid)
    bool wallpaper_on;          // SD wallpaper background on/off (wins over gradient)
    int  scrsaver_delay;        // Idle seconds before activation (0 = off)
    int  scrsaver_id;           // screensaver_id_t (see screensavers.h)
    bool sd_active;             // SD music player is the active source
    int  sd_index;              // Current track in the scanned queue
    int  sd_count;              // Tracks in the queue
    char sd_track[128];         // Current track file name
    char sd_dir[192];           // Folder of the current track
    bool sd_paused;             // SD playback paused
    bool sd_shuffle;            // Shuffle mode
    int  sd_repeat;             // Repeat mode: 0=none, 1=all, 2=one
    bool sd_show_screen;        // SD player show/hide screen in the nav ring
    bool radio_show_screen;     // radio show/hide screen in the nav ring
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
    bool has_bt_auto_switch;
    bool bt_auto_switch;
    bool has_bt_state;
    bt_state_t bt_state;
    bool has_bt_playing;
    bool bt_playing;
    bool has_bt_volume;
    int bt_volume;
    bool has_bt_vol_sync;
    bool bt_vol_sync;

    bool has_bt_title;
    const char *bt_title;
    bool has_bt_artist;
    const char *bt_artist;
    bool has_bt_duration_ms;
    int  bt_duration_ms;
    bool has_bt_position_s;
    int  bt_position_s;
    bool has_bt_codec;
    const char *bt_codec;
    bool has_bt_sample_rate;
    int  bt_sample_rate;
    bool has_bt_bits;
    int  bt_bits;

    bool has_time_synced;
    bool time_synced;

    bool has_screen;
    ui_screen_id_t screen;
    
    bool has_display_brightness;
    int display_brightness;

    bool has_theme;
    ui_theme_t theme;

    bool has_bg_gradient;
    bool bg_gradient;

    bool has_wallpaper_on;
    bool wallpaper_on;

    bool has_scrsaver_delay;
    int  scrsaver_delay;
    bool has_scrsaver_id;
    int  scrsaver_id;

    bool has_sd_active;
    bool sd_active;
    bool has_sd_index;
    int  sd_index;
    bool has_sd_count;
    int  sd_count;
    bool has_sd_track;
    const char *sd_track;
    bool has_sd_dir;
    const char *sd_dir;
    bool has_sd_paused;
    bool sd_paused;
    bool has_sd_shuffle;
    bool sd_shuffle;
    bool has_sd_repeat;
    int  sd_repeat;
    bool has_sd_show_screen;
    bool sd_show_screen;
    bool has_radio_show_screen;
    bool radio_show_screen;
} app_state_patch_t;

typedef void (*app_state_cb_t)(void);

void app_state_init(void);
app_state_t* app_state_get(void);
void app_state_subscribe(app_state_cb_t cb);
void app_state_update(const app_state_patch_t *patch);