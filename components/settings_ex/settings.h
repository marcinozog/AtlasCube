#pragma once

#include "esp_err.h"
#include "ui_events.h"
#include "app_state.h"
#include <stdbool.h>

typedef struct {
    int  volume;
    int  eq[10];
    bool eq_enabled;
} audio_settings_t;

typedef struct {
    int  curr_index;
    bool resume_on_boot;   // restore playback state across reboots (opt-in)
    bool was_playing;      // last known playing state, auto-persisted on play/stop
} playlist_settings_t;

typedef struct {
    bool enabled;
    int  dim_hour;
    int  dim_minute;
    int  dim_brightness;     // 0–100, applied between dim_time and bright_time
    int  bright_hour;
    int  bright_minute;

    // Night-mode audio actions tied to the same two edges (all optional).
    bool radio_off;          // at the evening (dim) edge: stop the radio
    bool radio_on;           // at the morning (bright) edge: start the radio
    int  radio_station;      // 0-based playlist index used when radio_on
    int  radio_volume;       // 0–100 target volume, reached via a fade-in
} dim_schedule_t;

typedef struct {
    ui_screen_id_t  screen;
    int             brightness;
    ui_theme_t      theme;
    bool            flip;            // rotate the whole screen 180° (applied live via display_set_flip)
    bool            invert;          // invert panel colours (INVON/INVOFF); XOR over the driver baseline — applied live
    bool            bg_gradient;     // dithered gradient background on/off
    bool            wallpaper_on;       // SD wallpaper background on/off (wins over gradient)
    char            wallpaper_path[64]; // full path to a panel-sized RGB565 .bin on SD
    char            logo_path[64];      // full path to a splash logo .bin on SD ("" = built-in)
    bool            show_boot_info;  // version + IP overlay on the splash (STA only)
    bool            sd_show_screen;  // show/hide SD player screen in the nav ring
    bool            radio_show_screen; // show/hide radio screen in the nav ring
    bool            show_fps;        // on-screen LVGL FPS + CPU% overlay (applied live)
    dim_schedule_t  dim_schedule;
} display_settings_t;

typedef struct {
    bool enable;                    // enable BT module
    bool show_screen;               // show/hide BT screen
    int volume;
    bool auto_switch;               // exclusive source: BT play stops radio & vice versa
    bool vol_sync;                  // sync module volume with the phone
} bluetooth_settings_t;

typedef struct {
    char server1[64];
    char server2[64];
    char tz[64];
} ntp_settings_t;

typedef struct {
    char ssid[64];
    char password[64];
} wifi_settings_t;

typedef struct {
    char hostname[32];   // mDNS hostname; empty = auto "atlascube-xxxx" from MAC
} device_settings_t;

typedef struct
{
    int  delay;            // seconds of idle before activating (0 = off)
    int  screensaver_id;   // screensaver_id_t (see screensavers.h)

    // dim screensaver (SCREENSAVER_DIM) — backlight level applied while idle
    int  dim_level;        // 0–100

    // photo-frame screensaver (SCREENSAVER_PHOTO)
    char photo_dir[64];    // SD directory with .bin slides (default "/sdcard/slides")
    int  photo_order;      // 0 = sequential, 1 = random
    int  photo_hold_s;     // seconds each slide stays on screen
    int  photo_effect;     // 0 topdown, 1 wipe, 2 dissolve, 3 interlaced, 4 random-per-slide
    int  photo_speed;      // reveal speed 1 (slow) .. 5 (fast)
    int  photo_clock;      // 0 = hide, 1 = overlay HH:MM clock on the photo
    int  photo_clock_size; // big-font size: 72 / 80 / 96 / 120
} scrsaver_settings_t;

typedef enum {
    DASHBOARD_VALUE_NUMBER = 0,
    DASHBOARD_VALUE_STRING = 1,
} dashboard_value_type_t;

typedef struct {
    char title[32];
    char url[256];
    char json_path[64];     // e.g. "rates[0].mid"; empty = use root
    char suffix[16];        // appended to value (may be empty)
    int  poll_interval_ms;  // minimum 5000

    // ── notification ──────────────────────────────────────────────────────
    bool   notify_enabled;
    int    value_type;           // dashboard_value_type_t

    // number mode
    bool   notify_num_low_en;
    double notify_num_low;
    bool   notify_num_high_en;
    double notify_num_high;

    // string mode
    bool   notify_str_eq_en;
    char   notify_str_eq[32];
    bool   notify_str_ne_en;
    char   notify_str_ne[32];
} dashboard_settings_t;


typedef struct {
    audio_settings_t     audio;
    playlist_settings_t  playlist;
    display_settings_t   display;
    bluetooth_settings_t bluetooth;
    ntp_settings_t       ntp;
    wifi_settings_t      wifi;
    scrsaver_settings_t  scrsaver;
    dashboard_settings_t dashboard;
    device_settings_t    device;
} app_settings_t;

esp_err_t settings_init(void);
void      settings_apply(void);
app_settings_t* settings_get(void);
esp_err_t settings_save(void);

void settings_set_volume(int volume);
void settings_set_eq_10(int *bands);
void settings_set_eq_enabled(bool enabled);
void settings_set_curr_index(int index);
void settings_set_resume_on_boot(bool enabled);
void settings_set_was_playing(bool playing);
void settings_set_screen(ui_screen_id_t screen);
void settings_set_brightness(int brightness);
void settings_set_night_schedule(const dim_schedule_t *ns);
void settings_set_bt_enable(bool enable);
void settings_set_bt_enable_volatile(bool enable);   // runtime switch, no SPIFFS write
void settings_set_bt_enable_quiet(bool enable);      // mux+state only, no broadcast/write
void settings_set_bt_show_screen(bool show);
void settings_set_bt_volume(int volume);
void settings_set_bt_auto_switch(bool enable);
void settings_set_bt_vol_sync(bool on);
void settings_set_ntp(const char *server1, const char *server2, const char *tz);
void settings_set_theme(ui_theme_t theme);
void settings_set_flip(bool enabled);
void settings_set_invert(bool enabled);
void settings_set_show_fps(bool enabled);
void settings_set_bg_gradient(bool enabled);
void settings_set_wallpaper(bool on, const char *path);
void settings_set_logo_path(const char *path);
void settings_set_show_boot_info(bool enabled);
void settings_set_sd_show_screen(bool show);
void settings_set_radio_show_screen(bool show);
void settings_set_wifi(const char *ssid, const char *password);
void settings_set_hostname(const char *hostname);
void settings_set_scrsaver_delay(int delay);
void settings_set_scrsaver_id(int id);
void settings_set_scrsaver_dim_level(int level);
void settings_set_photo(const char *dir, int order, int hold_s, int effect, int speed,
                        int clock, int clock_size);
// Bumped on every settings_set_photo() — lets the running photo screensaver
// detect a config change and apply it live, without a re-entry.
unsigned settings_photo_generation(void);
void settings_set_dashboard(const char *title,
                            const char *url,
                            const char *json_path,
                            const char *suffix,
                            int poll_interval_ms);

// Notification fields are applied by http_server directly via the mutable
// pointer returned by settings_get(), followed by settings_save().
