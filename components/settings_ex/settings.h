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
    int curr_index;
} playlist_settings_t;

typedef struct {
    ui_screen_id_t  screen;
    int             brightness;
    ui_theme_t      theme;
} display_settings_t;

typedef struct {
    bool enable;                    // enable BT module
    bool show_screen;               // show/hide BT screen         
    int volume;
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

typedef struct
{
    int  delay;            // seconds of idle before activating (0 = off)
    int  screensaver_id;   // screensaver_id_t (see screensavers.h)
} scrsaver_settings_t;

typedef struct {
    char title[32];
    char url[256];
    char json_path[64];     // e.g. "rates[0].mid"; empty = use root
    char suffix[16];        // appended to value (may be empty)
    int  poll_interval_ms;  // minimum 5000
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
} app_settings_t;

esp_err_t settings_init(void);
void      settings_apply(void);
app_settings_t* settings_get(void);
esp_err_t settings_save(void);

void settings_set_volume(int volume);
void settings_set_eq_10(int *bands);
void settings_set_eq_enabled(bool enabled);
void settings_set_curr_index(int index);
void settings_set_screen(ui_screen_id_t screen);
void settings_set_brightness(int brightness);
void settings_set_bt_enable(bool enable);
void settings_set_bt_show_screen(bool show);
void settings_set_bt_volume(int volume);
void settings_set_ntp(const char *server1, const char *server2, const char *tz);
void settings_set_theme(ui_theme_t theme);
void settings_set_wifi(const char *ssid, const char *password);
void settings_set_scrsaver_delay(int delay);
void settings_set_scrsaver_id(int id);
void settings_set_dashboard(const char *title,
                            const char *url,
                            const char *json_path,
                            const char *suffix,
                            int poll_interval_ms);
