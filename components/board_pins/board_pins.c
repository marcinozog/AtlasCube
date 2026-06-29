#include "board_pins.h"
#include "defines.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "PINS";
#define PINMAP_NS "pinmap"

board_pins_t g_pins;

// Single source of truth for NVS key ↔ struct field. APPEND-ONLY (see header):
// new pins go at the end here AND at the end of board_pins_t, never reordered.
static const struct { const char *key; size_t off; } k_map[] = {
    { "lcd_mosi", offsetof(board_pins_t, lcd_mosi) },
    { "lcd_clk",  offsetof(board_pins_t, lcd_clk)  },
    { "lcd_cs",   offsetof(board_pins_t, lcd_cs)   },
    { "lcd_dc",   offsetof(board_pins_t, lcd_dc)   },
    { "lcd_rst",  offsetof(board_pins_t, lcd_rst)  },
    { "lcd_led",  offsetof(board_pins_t, lcd_led)  },
    { "qspi_cs",  offsetof(board_pins_t, qspi_cs)  },
    { "qspi_clk", offsetof(board_pins_t, qspi_clk) },
    { "qspi_d0",  offsetof(board_pins_t, qspi_d0)  },
    { "qspi_d1",  offsetof(board_pins_t, qspi_d1)  },
    { "qspi_d2",  offsetof(board_pins_t, qspi_d2)  },
    { "qspi_d3",  offsetof(board_pins_t, qspi_d3)  },
    { "qspi_rst", offsetof(board_pins_t, qspi_rst) },
    { "ctp_scl",  offsetof(board_pins_t, ctp_scl)  },
    { "ctp_sda",  offsetof(board_pins_t, ctp_sda)  },
    { "ctp_int",  offsetof(board_pins_t, ctp_int)  },
    { "ctp_rst",  offsetof(board_pins_t, ctp_rst)  },
    { "sd_clk",   offsetof(board_pins_t, sd_clk)   },
    { "sd_cmd",   offsetof(board_pins_t, sd_cmd)   },
    { "sd_d0",    offsetof(board_pins_t, sd_d0)    },
    { "sd_cd",    offsetof(board_pins_t, sd_cd)    },
    { "i2s_data", offsetof(board_pins_t, i2s_data) },
    { "i2s_bck",  offsetof(board_pins_t, i2s_bck)  },
    { "i2s_lck",  offsetof(board_pins_t, i2s_lck)  },
    { "enc_clk",  offsetof(board_pins_t, enc_clk)  },
    { "enc_dt",   offsetof(board_pins_t, enc_dt)   },
    { "enc_btn",  offsetof(board_pins_t, enc_btn)  },
    { "buzzer",   offsetof(board_pins_t, buzzer)   },
    { "bt_pin",   offsetof(board_pins_t, bt_pin)   },
    { "bt_tx",    offsetof(board_pins_t, bt_tx)    },
    { "bt_rx",    offsetof(board_pins_t, bt_rx)    },
    { "tp_clk",   offsetof(board_pins_t, tp_clk)   },
    { "tp_mosi",  offsetof(board_pins_t, tp_mosi)  },
    { "tp_miso",  offsetof(board_pins_t, tp_miso)  },
    { "tp_cs",    offsetof(board_pins_t, tp_cs)    },
    { "tp_irq",   offsetof(board_pins_t, tp_irq)   },
};
#define PIN_COUNT (sizeof(k_map) / sizeof(k_map[0]))

static int8_t *field_at(size_t i)
{
    return (int8_t *)((uint8_t *)&g_pins + k_map[i].off);
}

// Defaults from defines.h. Everything starts at -1 (disabled); only the pins the
// active variant actually defines get a real value, so e.g. an SPI build leaves
// qspi_* at -1 and vice versa.
static void load_defaults(void)
{
    memset(&g_pins, 0xFF, sizeof(g_pins));   // all int8_t fields → -1

#ifdef LCD_PIN_MOSI
    g_pins.lcd_mosi = LCD_PIN_MOSI;
    g_pins.lcd_clk  = LCD_PIN_CLK;
    g_pins.lcd_cs   = LCD_PIN_CS;
    g_pins.lcd_dc   = LCD_PIN_DC;
    g_pins.lcd_rst  = LCD_PIN_RST;
#endif
#ifdef LCD_LED
    g_pins.lcd_led  = LCD_LED;
#endif
#ifdef DISPLAY_PIN_CS
    g_pins.qspi_cs  = DISPLAY_PIN_CS;
    g_pins.qspi_clk = DISPLAY_PIN_CLK;
    g_pins.qspi_d0  = DISPLAY_PIN_D0;
    g_pins.qspi_d1  = DISPLAY_PIN_D1;
    g_pins.qspi_d2  = DISPLAY_PIN_D2;
    g_pins.qspi_d3  = DISPLAY_PIN_D3;
    g_pins.qspi_rst = DISPLAY_PIN_RST;
#endif

    g_pins.ctp_scl  = CTP_SCL;
    g_pins.ctp_sda  = CTP_SDA;
    g_pins.ctp_int  = CTP_INT;
    g_pins.ctp_rst  = CTP_RST;

#ifdef TP_CS
    // XPT2046 SPI touch (only defined by variants using it; tp_clk/tp_mosi = -1
    // means "share the LCD SPI bus"). Fields stay -1 otherwise.
    g_pins.tp_clk   = TP_CLK;
    g_pins.tp_mosi  = TP_MOSI;
    g_pins.tp_miso  = TP_MISO;
    g_pins.tp_cs    = TP_CS;
    g_pins.tp_irq   = TP_IRQ;
#endif

    g_pins.sd_clk   = SD_PIN_CLK;
    g_pins.sd_cmd   = SD_PIN_CMD;
    g_pins.sd_d0    = SD_PIN_D0;
    g_pins.sd_cd    = SD_PIN_CD;

    g_pins.i2s_data = I2S_DATA;
    g_pins.i2s_bck  = I2S_BCK;
    g_pins.i2s_lck  = I2S_LCK;

    g_pins.enc_clk  = ENC_CLK_PIN;
    g_pins.enc_dt   = ENC_DT_PIN;
    g_pins.enc_btn  = ENC_BTN_PIN;

    g_pins.buzzer   = BUZZER_PIN;

    g_pins.bt_pin   = BT_MOULE_PIN;
    g_pins.bt_tx    = BT_MODULE_TX_PIN;
    g_pins.bt_rx    = BT_MODULE_RX_PIN;
}

void board_pins_load(void)
{
    load_defaults();

    nvs_handle_t h;
    if (nvs_open(PINMAP_NS, NVS_READONLY, &h) != ESP_OK) {
        ESP_LOGI(TAG, "no pinmap namespace — using compiled defaults");
        return;
    }

    int applied = 0;
    for (size_t i = 0; i < PIN_COUNT; i++) {
        int8_t v;
        if (nvs_get_i8(h, k_map[i].key, &v) == ESP_OK) {
            *field_at(i) = v;
            ESP_LOGI(TAG, "override %s = %d", k_map[i].key, v);
            applied++;
        }
    }
    nvs_close(h);
    ESP_LOGI(TAG, "pinmap loaded (%d override%s)", applied, applied == 1 ? "" : "s");
}

esp_err_t board_pins_set(const char *key, int val)
{
    if (val < -1 || val > 48) return ESP_ERR_INVALID_ARG;

    size_t idx = PIN_COUNT;
    for (size_t i = 0; i < PIN_COUNT; i++) {
        if (strcmp(k_map[i].key, key) == 0) { idx = i; break; }
    }
    if (idx == PIN_COUNT) return ESP_ERR_INVALID_ARG;

    nvs_handle_t h;
    esp_err_t err = nvs_open(PINMAP_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;

    err = nvs_set_i8(h, key, (int8_t)val);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);

    if (err == ESP_OK) ESP_LOGI(TAG, "saved %s = %d (effective next boot)", key, val);
    return err;
}

esp_err_t board_pins_reset(void)
{
    nvs_handle_t h;
    // READWRITE creates the namespace if absent; erase_all then no-ops cleanly.
    esp_err_t err = nvs_open(PINMAP_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;

    err = nvs_erase_all(h);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);

    if (err == ESP_OK) ESP_LOGI(TAG, "pinmap reset to defaults (effective next boot)");
    return err;
}

// I2S pin accessors. The ESP-ADF audio_board's board_pins_config.c (get_i2s_pins)
// reads pins through these — it lives inside the ADF audio_board component and
// can't include this header or REQUIRE this component, so it forward-declares
// these functions and they resolve at link time (board_pins.o is always linked,
// main depends on it). g_pins is loaded long before audio init, so it's valid.
int board_pins_i2s_bck(void)  { return g_pins.i2s_bck;  }
int board_pins_i2s_lck(void)  { return g_pins.i2s_lck;  }
int board_pins_i2s_data(void) { return g_pins.i2s_data; }

size_t board_pins_count(void) { return PIN_COUNT; }

const char *board_pins_key(size_t i) { return i < PIN_COUNT ? k_map[i].key : NULL; }

int board_pins_get(size_t i) { return i < PIN_COUNT ? *field_at(i) : -1; }
