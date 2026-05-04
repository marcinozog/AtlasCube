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

typedef struct {
    audio_settings_t     audio;
    playlist_settings_t  playlist;
    display_settings_t   display;
    bluetooth_settings_t bluetooth;
    ntp_settings_t       ntp;
    wifi_settings_t      wifi;
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
