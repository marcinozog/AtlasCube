#define WEB_ROOT "/spiffs"
#define SETTINGS_FILE "/spiffs/settings.json"
#define PLAYLIST_FILE "/spiffs/data/playlist.csv"
#define THEME_FILE "/spiffs/theme.json"
#define EVENTS_FILE "/spiffs/events.json"


// ===== DISPLAY =====

#define LCD_PIN_MOSI  11
#define LCD_PIN_CLK   12
#define LCD_PIN_CS    41 // WTD -> 37 -> PSRAM 33, 34, 35, 36, 37
#define LCD_PIN_DC    39
#define LCD_PIN_RST   38
#define LCD_LED       40

#define LCD_HOST      SPI2_HOST
#define LCD_WIDTH     320
#define LCD_HEIGHT    240



// ===== I2S PCM5102A =====

// defined in components\audio_board\esp32_s3_atlascube\board_def.h (ESP-ADF requirement)

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

#define ENC_CLK         3
#define ENC_DT          9
#define ENC_BTN         10


// ===== BUZZER =====

#define BUZZER_GPIO     2


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


