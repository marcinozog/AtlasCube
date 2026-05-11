#pragma once

#include "sdkconfig.h"

#define WEB_ROOT "/spiffs"
#define SETTINGS_FILE "/spiffs/settings.json"
#define PLAYLIST_FILE "/spiffs/data/playlist.csv"
#define THEME_FILE "/spiffs/theme.json"
#define EVENTS_FILE "/spiffs/events.json"

// WTD -> 37 -> PSRAM 33, 34, 35, 36, 37

// ===== USER CONFIG =====
// UI layout: pick UI_PROFILE_* below
// Display driver: pick CONFIG_DISPLAY_* in sdkconfig.defaults (or via menuconfig)


// ===== UI =====

#define UI_PROFILE_240X296
// #define UI_PROFILE_320x240
// #define UI_PROFILE_MONO_128X64


// ===== DISPLAY =====
// DISPLAY_WIDTH / DISPLAY_HEIGHT live in ui_profile.h, derived from UI_PROFILE_*

#define DISPLAY_HOST      SPI2_HOST
#define DISPLAY_CLK_SPEED   40000000

#if CONFIG_DISPLAY_ILI9341

#define LCD_PIN_MOSI  11
#define LCD_PIN_CLK   12
#define LCD_PIN_CS    41
#define LCD_PIN_DC    39
#define LCD_PIN_RST   38
#define LCD_LED       40

#elif CONFIG_DISPLAY_CO5300

#define DISPLAY_PIN_CS      2
#define DISPLAY_PIN_CLK     38
#define DISPLAY_PIN_D0      39
#define DISPLAY_PIN_D1      40
#define DISPLAY_PIN_D2      41
#define DISPLAY_PIN_D3      42
#define DISPLAY_PIN_RST     1

#else
    #error "Unknown DISPLAY_TYPE"
#endif



// ===== I2S PCM5102A =====

// Defined in components\audio_board\esp32_s3_atlascube\board_def.h (ESP-ADF requirement)

// #define I2S_DATA        16 // GPIO_NUM_8
// #define I2S_BCK         15 // GPIO_NUM_7
// #define I2S_LCK         17 // GPIO_NUM_9


// ===== RADIO =====

#define RADIO_VOL_STEP 3


// ===== BLUETOOTH =====

#define BT_MOULE_PIN            18
#define BT_MODULE_TX_PIN        4
#define BT_MODULE_RX_PIN        5

#define BT_UART_NUM             UART_NUM_1
#define BT_UART_BAUD            115200

#define BT_AT_SVOL_MAX          15
#define BT_VOL_STEP 5


// ===== ENCODER =====

#define ENC_CLK_PIN         3
#define ENC_DT_PIN          9
#define ENC_BTN_PIN         10


// ===== BUZZER =====

#define BUZZER_PIN     6 //2


// ===== WIFI =====

#define WIFI_AP_SSID             "AtlasCube"
#define WIFI_AP_PASS             "99876543"


// ===== NTP =====

#define DEFAULT_NTP_SERVER1  "pool.ntp.org"
#define DEFAULT_NTP_SERVER2  "time.cloudflare.com"
// POSIX TZ string: STD offset DST, DST start, DST end
// CET-1CEST,M3.5.0,M10.5.0/3
// CET-1    → standard time UTC+1
// CEST     → daylight time (implicit UTC+2, i.e. +1h from standard)
// M3.5.0/2  → DST start: month 3 (Mar), week 5 (last), day 0 (Sun), at 02:00
// M10.5.0/3 → DST end:   month 10 (Oct), week 5 (last), day 0 (Sun), at 03:00
#define DEFAULT_TZ_STRING    "CET-1CEST,M3.5.0/2,M10.5.0/3"


