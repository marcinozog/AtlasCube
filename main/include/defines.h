#pragma once

#include "sdkconfig.h"

// www partition (editable UI) is mounted at /spiffs; user data (settings/theme/
// events/mqtt JSON plus the station list) lives on a separate `config` partition
// mounted at /config so a www update (re-upload/re-flash) can't clobber it. The
// playlist is user data too, so it lives on /config; the default ships in the
// config image and is seeded once from the old /spiffs location (see playlist.c).
#define WEB_ROOT "/spiffs"
#define CONFIG_ROOT "/config"
#define SETTINGS_FILE "/config/settings.json"
#define PLAYLIST_FILE "/config/playlist.csv"
#define PLAYLIST_FILE_LEGACY "/spiffs/playlist.csv"   // pre-move location, for one-time seed
#define THEME_FILE "/config/theme.json"
#define EVENTS_FILE "/config/events.json"

// ===== GPIO MAP (ESP32-S3) =====
// In-package / reserved — never route to a peripheral:
//   0          BOOT strapping
//   19, 20     USB D-/D+ (native USB-CDC console)
//   26–32      SPI flash (in-package)
//   33–37      Octal PSRAM (in-package)
//   43, 44     UART0 console TX/RX (idf.py monitor)
//   45, 46     strapping pins (VDD_SPI / boot mode)
//
// Allowed pins (broken out on the PCB) — assign freely per variant:
//   1  2  3  4  5  6  7  8  9  10  11  12  13  14  15  16  17  18
//   38  39  40  41  42  47  48

// ===== USER CONFIG =====
// Three independent hardware choices below — uncomment exactly one in each group.
// CMake reads this file and auto-generates sdkconfig.variant — no need to touch sdkconfig.
// After changing a variant: `idf.py fullclean && idf.py build`


// ===== FLASH SIZE =====
// Only the 16 MB dual-slot layout is supported (all boards ship with 16 MB).
#define FLASH_16MB


// ===== DISPLAY DRIVER =====
#define DISPLAY_ILI9341
// #define DISPLAY_ST7796
// #define DISPLAY_ILI9488
// #define DISPLAY_CO5300
// #define DISPLAY_SSD1322



// ===== UI PROFILE =====
// #define UI_PROFILE_240X296
#define UI_PROFILE_320x240
// #define UI_PROFILE_480x320
// #define UI_PROFILE_MONO_128X64
// #define UI_PROFILE_MONO_256X64


// ===== TOUCH DRIVER =====
#define TOUCH_FT6336U
// #define TOUCH_CST816D
// XPT2046 = resistive SPI touch (shares LCD bus or dedicated SPI3)
// #define TOUCH_XPT2046
// #define TOUCH_NONE


// ===== DISPLAY =====
// DISPLAY_WIDTH / DISPLAY_HEIGHT live in ui_profile.h, derived from UI_PROFILE_*

#define DISPLAY_HOST      SPI2_HOST
#define DISPLAY_CLK_SPEED   40000000

// On-screen FPS + CPU% overlay (bottom-right). 1 = show, 0 = hide at runtime.
// The monitor is always compiled in (needs CONFIG_LV_USE_SYSMON/OBSERVER/PERF_MONITOR
// in sdkconfig.defaults); this flag only toggles visibility. To drop it entirely
// (and its tiny measurement timer) turn those three CONFIG_LV_USE_* off instead.
#define UI_SHOW_FPS_OVERLAY 1

#if CONFIG_DISPLAY_ILI9341

#define LCD_PIN_MOSI  39
#define LCD_PIN_CLK   40
#define LCD_PIN_CS    41
#define LCD_PIN_DC    2
#define LCD_PIN_RST   42
#define LCD_LED       1

#elif CONFIG_DISPLAY_ST7796

// ST7796U 480x320 — same wiring as ILI9341
#define LCD_PIN_MOSI  39
#define LCD_PIN_CLK   40
#define LCD_PIN_CS    41
#define LCD_PIN_DC    2
#define LCD_PIN_RST   42
#define LCD_LED       1

#elif CONFIG_DISPLAY_ILI9488

// ILI9488 480x320 — same wiring as ILI9341 / ST7796.
// 18-bit RGB666 over 4-wire SPI (the panel can't do RGB565 in serial mode).
#define LCD_PIN_MOSI  39
#define LCD_PIN_CLK   40
#define LCD_PIN_CS    41
#define LCD_PIN_DC    2
#define LCD_PIN_RST   42
#define LCD_LED       1

#elif CONFIG_DISPLAY_CO5300

#define DISPLAY_PIN_CS      2
#define DISPLAY_PIN_CLK     38
#define DISPLAY_PIN_D0      39
#define DISPLAY_PIN_D1      40
#define DISPLAY_PIN_D2      41
#define DISPLAY_PIN_D3      42
#define DISPLAY_PIN_RST     1

#elif CONFIG_DISPLAY_SSD1322

// SSD1322 256x64 4-wire SPI mono OLED — no backlight pin (contrast-controlled).
#define LCD_PIN_MOSI  39
#define LCD_PIN_CLK   40
#define LCD_PIN_CS    41
#define LCD_PIN_DC    2
#define LCD_PIN_RST   42

#else
    #error "Unknown DISPLAY_TYPE"
#endif


// ===== TOUCH =====
// Driver selected via CONFIG_TOUCH_* in sdkconfig.defaults (CST816D / FT6336U / XPT2046)
#define CTP_SCL     47
#define CTP_SDA     48
#define CTP_INT     -1
#define CTP_RST     -1

#if CONFIG_TOUCH_XPT2046
// XPT2046 resistive touch (SPI). Defaults share the LCD SPI bus (SPI2): leave
// TP_CLK/TP_MOSI = -1 to reuse LCD_PIN_CLK/LCD_PIN_MOSI, and wire only CS/MISO/IRQ.
// Set TP_CLK/TP_MOSI to real pins to put the controller on a dedicated SPI3 bus.
#define TP_CLK      -1
#define TP_MOSI     -1
#define TP_MISO     47 //45
#define TP_CS       48 //46
#define TP_IRQ      -1   // PENIRQ, -1 = polled
#endif




// ===== SD CARD (SDMMC 1-bit) =====
// 3 lines: CLK + CMD + D0. CMD and D0 need ~10k external pull-ups on the PCB.
#define HAS_SD_CARD
#define SD_PIN_CLK   12
#define SD_PIN_CMD   13
#define SD_PIN_D0    11
#define SD_PIN_CD    -1   // card-detect, optional (-1 = none)


// ===== I2S PCM5102A =====
// Single source of truth. ESP-ADF reads these via board_def.h, which includes
// this header (components\audio_board\esp32_s3_atlascube\board_def.h).

#define I2S_DATA        16
#define I2S_BCK         15
#define I2S_LCK         17


// ===== RADIO =====

#define RADIO_VOL_STEP 3


// ===== BLUETOOTH =====

#define BT_MOULE_PIN            18
#define BT_MODULE_TX_PIN        4
#define BT_MODULE_RX_PIN        5

#define BT_UART_NUM             UART_NUM_1
#define BT_UART_BAUD            115200

// Active BT module (selects the AT dialect descriptor, see bt_module.h).
#define BT_MODULE_QCC5125V2

#define BT_VOL_STEP 5           // UI volume step in % (not the module's SVSTEP)


// ===== ENCODER =====

#define ENC_CLK_PIN         3
#define ENC_DT_PIN          9
#define ENC_BTN_PIN         10


// ===== BUZZER =====

#define BUZZER_PIN     6   // set to -1 to disable the buzzer


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


