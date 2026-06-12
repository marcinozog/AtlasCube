#pragma once

#include "sdkconfig.h"

#define WEB_ROOT "/spiffs"
#define SETTINGS_FILE "/spiffs/settings.json"
#define PLAYLIST_FILE "/spiffs/data/playlist.csv"
#define THEME_FILE "/spiffs/theme.json"
#define EVENTS_FILE "/spiffs/events.json"

// WTD -> 37 -> PSRAM 33, 34, 35, 36, 37

// ===== USER CONFIG =====

// ===== BOARD SELECT =====
// Uncomment exactly one board variant.
// After changing: `idf.py fullclean && idf.py build`
// #define BOARD_ATLASCUBE
#define BOARD_ES3C28P

// Three independent hardware choices below — uncomment exactly one in each group.
// CMake reads this file and auto-generates sdkconfig.variant — no need to touch sdkconfig.
// After changing a variant: `idf.py fullclean && idf.py build`


// ===== FLASH SIZE =====
// #define FLASH_8MB
#define FLASH_16MB


// ===== DISPLAY DRIVER =====
#define DISPLAY_ILI9341
// #define DISPLAY_ST7796
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
// #define TOUCH_NONE


// ===== DISPLAY =====
// DISPLAY_WIDTH / DISPLAY_HEIGHT live in ui_profile.h, derived from UI_PROFILE_*

#define DISPLAY_HOST      SPI2_HOST
#define DISPLAY_CLK_SPEED   40000000

#if defined(BOARD_ATLASCUBE)

#if CONFIG_DISPLAY_ILI9341

#define LCD_PIN_MOSI  39
#define LCD_PIN_CLK   40
#define LCD_PIN_CS    41
#define LCD_PIN_DC    2 //39
#define LCD_PIN_RST   42 //38
#define LCD_LED       1 //40

#elif CONFIG_DISPLAY_ST7796

// ST7796U 480x320 — same wiring as ILI9341
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

#elif defined(BOARD_ES3C28P)

// ES3C28P — QD2833 2.8" SPI LCD panel via QD1 18-pin connector
// LCD_SCK=GPIO12, LCD_RS(DC)=GPIO46, LCD_CS=GPIO10, LCD_MOSI=GPIO11, LCD_MISO=GPIO13
// LCD_BL (backlight HIGH active) = GPIO45  [LCD_BL via Q4 BSS138]
// LCD_RST: wired to global CHIP_PU (EN), no separate pin — pass -1 to driver
#define LCD_PIN_CS    10   // LCD_CS
#define LCD_PIN_MOSI  11   // LCD_MOSI
#define LCD_PIN_CLK   12   // LCD_SCK
#define LCD_PIN_MISO  13   // LCD_MISO
#define LCD_PIN_DC    46   // LCD_RS (Data/Command)
#define LCD_PIN_RST   -1   // wired to CHIP_PU — no soft reset
#define LCD_LED       45   // LCD_BL (active HIGH)

#endif  // BOARD_* display


// ===== TOUCH =====
// Driver selected via CONFIG_TOUCH_* in sdkconfig.defaults (CST816D / FT6336U)

#if defined(BOARD_ATLASCUBE)
#define CTP_SCL     47
#define CTP_SDA     48
#define CTP_INT     -1
#define CTP_RST     -1
#elif defined(BOARD_ES3C28P)
// Touch controller shares I2C bus with audio ES8311 (IIC Slave 0x38)
// AU_SCL / IIC_SCL / CTP_SCL → GPIO15   AU_SDA / IIC_SDA / CTP_SDA → GPIO16
#define CTP_SCL     15   // AU_SCL / IIC_SCL / CTP_SCL
#define CTP_SDA     16   // AU_SDA / IIC_SDA / CTP_SDA
#define CTP_INT     17   // CTP_INT
#define CTP_RST     18   // CTP_RST
#endif


// ===== SD CARD (SDMMC 1-bit) =====
// components/sdcard runs in 1-bit mode (CLK + CMD + D0 + CD). CMD and D0
// need ~10k external pull-ups on the PCB.
#if defined(BOARD_ATLASCUBE)
// On CO5300 builds pins 38/39/40 are taken by the QSPI display — pick others.
#define HAS_SD_CARD
#define SD_PIN_CLK   12
#define SD_PIN_CMD   13
#define SD_PIN_D0    11
#define SD_PIN_CD    -1   // card-detect, optional (-1 = none)
#elif defined(BOARD_ES3C28P)
// ES3C28P routes a dedicated SDIO 4-bit lane (CLK=38, CMD=40, D0..D3=39/41/48/47)
// but the current driver only uses 1-bit — we wire CLK/CMD/D0 and leave the
// other three GPIOs free. The PCB still has them connected to the card slot,
// so a future 4-bit upgrade just needs widening the driver.
//
// NOTE: GPIO36/37 are reserved by the on-package OPI PSRAM (SPICLK_N/P) and
// must NOT be used by application peripherals — picking them for SD signals
// would corrupt PSRAM at the first SD transaction.
#define HAS_SD_CARD
#define SD_PIN_CLK   38
#define SD_PIN_CMD   40
#define SD_PIN_D0    39
#define SD_PIN_CD    -1
#endif


// ===== I2S =====
// Single source of truth. ESP-ADF reads these via board_def.h, which includes
// this header (components\audio_board\esp32_s3_atlascube\board_def.h).

#if defined(BOARD_ATLASCUBE)
// PCM5102A (passive DAC, no codec chip)
#define I2S_DATA        16   // I2S Data Out
#define I2S_BCK         15   // I2S Bit Clock
#define I2S_LCK         17   // I2S Word Select (LRCLK)
#elif defined(BOARD_ES3C28P)
// ES8311 audio codec — I2S pins.
//
// IMPORTANT — DATA / DI ARE SWAPPED vs. the original ES32C28P.md spec!
// The spec lists I2S_DO=6 / I2S_DI=8, but the actual hardware (verified
// against the LCD-Wiki "music.ino" demo that works on this exact board,
// and consistent with the schematic signal routing) has:
//   ESP32 GPIO8 → codec pin 9 (DSDIN) — DAC playback input
//   ESP32 GPIO6 ← codec pin 7 (ASDOUT) — microphone capture output
// Driving I2S_DATA on GPIO6 produced ~30-40 dB of attenuated audio
// (just EMI/crosstalk from BCLK/LRCK on neighboring pins) because the
// real DAC input pin (GPIO8) was getting no signal AND we were fighting
// the codec's ADC output on the same pin.
#define I2S_MCK         4    // Master Clock (I2S_MCK) — codec pin 2
#define I2S_BCK         5    // Serial Clock / BCLK (I2S_SCK) — codec pin 6
#define I2S_DATA        8    // Data Out → codec DSDIN (pin 9) — was 6 (wrong)
#define I2S_LCK         7    // Word Select / LRCK (I2S_LRC) — codec pin 8
#define I2S_DI          6    // Data In ← codec ASDOUT (pin 7) — was 8 (wrong)
// SC8002B PA enable — per ES32C28P spec AND confirmed by testing:
// LOW = amp enabled, HIGH = shutdown. We deliberately do NOT expose
// this via ADF's get_pa_enable_gpio() because i2s_stream_start would
// drive it HIGH (its convention) and mute the amp on every play.
#define AUDIO_EN        1    // PA enable: GPIO1 active LOW (LOW = on)
#endif


// ===== RADIO =====

#define RADIO_VOL_STEP 3


// ===== BLUETOOTH =====

#if defined(BOARD_ATLASCUBE)
#define BT_MOULE_PIN            18
#define BT_MODULE_TX_PIN        4
#define BT_MODULE_RX_PIN        5
#elif defined(BOARD_ES3C28P)
// No external BT module on ES3C28P
#define BT_MOULE_PIN            -1
#define BT_MODULE_TX_PIN        -1
#define BT_MODULE_RX_PIN        -1
#endif

#define BT_UART_NUM             UART_NUM_1
#define BT_UART_BAUD            115200

#define BT_AT_SVOL_MAX          15
#define BT_VOL_STEP 5


// ===== ENCODER =====

#if defined(BOARD_ATLASCUBE)
#define ENC_CLK_PIN         3
#define ENC_DT_PIN          9
#define ENC_BTN_PIN         10
#elif defined(BOARD_ES3C28P)
// No rotary encoder on ES3C28P
#define ENC_CLK_PIN         -1
#define ENC_DT_PIN          -1
#define ENC_BTN_PIN         -1
#endif


// ===== BUZZER =====

#if defined(BOARD_ATLASCUBE)
#define BUZZER_PIN     6 //2
#elif defined(BOARD_ES3C28P)
#define BUZZER_PIN     -1
#endif


// ===== RGB LED + BATTERY ADC =====
// Use -1 on boards where the feature is absent. Drivers (components/rgb_led,
// components/battery) check for >=0 at init and skip cleanly otherwise.

#if defined(BOARD_ES3C28P)
#define RGB_LED_PIN     42   // WS2812B (XL-5050RGBC) on GPIO42
#define BAT_ADC_PIN      9   // Battery voltage divider ADC → GPIO9
                             // Hardware divider is 1:1 (R1 = R2), so the
                             // raw measurement is half of Vbat. battery.c
                             // multiplies the mV reading by 2 internally.
#elif defined(BOARD_ATLASCUBE)
#define RGB_LED_PIN     -1   // No addressable LED on AtlasCube
#define BAT_ADC_PIN     -1   // No battery on AtlasCube (USB-powered)
#endif


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
