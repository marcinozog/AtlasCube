#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

// ─────────────────────────────────────────────────────────────────────────────
// Runtime pin map.
//
// Defaults come from main/include/defines.h (compile-time, per HW variant); any
// key present in the NVS "pinmap" namespace overrides its default at boot. This
// lets one firmware binary (fixed display/touch DRIVER) drive boards with
// different wiring — pins are configured on-device via the setup page, no
// rebuild and no per-board binary.
//
// FORWARD-COMPAT RULE — read before touching this struct:
//   Only ever ADD fields at the END, and only ever ADD new NVS keys (in the
//   table in board_pins.c). NEVER rename, remove, or reorder existing ones.
//   A key absent from NVS falls back to its compiled default, so:
//     - new firmware tolerates a pinmap written by an older firmware (missing
//       keys → defaults), and
//     - old firmware ignores keys it doesn't know.
//   That is the whole reason this is NVS key/value and not a packed struct blob.
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
    // Display — SPI variants (ili9341 / st7796 / ili9488 / ssd1322)
    int8_t lcd_mosi;
    int8_t lcd_clk;
    int8_t lcd_cs;
    int8_t lcd_dc;
    int8_t lcd_rst;
    int8_t lcd_led;     // backlight (-1 = none, e.g. mono OLED)

    // Display — QSPI variant (co5300)
    int8_t qspi_cs;
    int8_t qspi_clk;
    int8_t qspi_d0;
    int8_t qspi_d1;
    int8_t qspi_d2;
    int8_t qspi_d3;
    int8_t qspi_rst;

    // Touch (I2C)
    int8_t ctp_scl;
    int8_t ctp_sda;
    int8_t ctp_int;     // -1 = polled
    int8_t ctp_rst;     // -1 = none

    // SD card (SDMMC 1-bit)
    int8_t sd_clk;
    int8_t sd_cmd;
    int8_t sd_d0;
    int8_t sd_cd;       // card-detect, -1 = none

    // I2S DAC (PCM5102A)
    int8_t i2s_data;
    int8_t i2s_bck;
    int8_t i2s_lck;

    // Rotary encoder
    int8_t enc_clk;
    int8_t enc_dt;
    int8_t enc_btn;

    // Buzzer (-1 = disabled)
    int8_t buzzer;

    // Bluetooth module UART
    int8_t bt_pin;
    int8_t bt_tx;
    int8_t bt_rx;
    // ── APPEND NEW PINS BELOW THIS LINE ONLY ──
} board_pins_t;

// The single live pin map. Valid after board_pins_load().
extern board_pins_t g_pins;

// Fill g_pins with compiled defaults (from defines.h), then apply NVS overrides.
// Call once, early in app_main: after nvs_flash_init(), before any peripheral
// init that reads g_pins.
void board_pins_load(void);

// Persist one pin to the NVS "pinmap" namespace. key = struct field name (see
// the table in board_pins.c). val in [-1, 48]; -1 = disabled. Takes effect on
// next boot. Returns ESP_ERR_INVALID_ARG for an unknown key or out-of-range val.
esp_err_t board_pins_set(const char *key, int val);

// Clear the whole NVS "pinmap" namespace → next boot uses compiled defaults.
// The clean way to undo experiments without erasing the rest of NVS (Wi-Fi etc.).
esp_err_t board_pins_reset(void);

// Read-only iteration over the live pin map — lets the web layer build/return
// JSON without depending on the field list. Index in [0, board_pins_count()).
size_t      board_pins_count(void);
const char *board_pins_key(size_t i);   // NULL if out of range
int         board_pins_get(size_t i);   // current value, -1 if out of range

// I2S pin accessors for the ESP-ADF audio_board (board_pins_config.c), which
// can't include this header — it forward-declares these (see board_pins.c).
int board_pins_i2s_bck(void);
int board_pins_i2s_lck(void);
int board_pins_i2s_data(void);
