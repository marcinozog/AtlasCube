#include "board.h"
#include "board_pins_config.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "BOARD_PINS";

// I2S pins are runtime-configurable (NVS "pinmap", see components/board_pins).
// This file compiles inside the ESP-ADF audio_board component, which can't
// REQUIRE the app's board_pins component, so forward-declare its accessors;
// g_pins is resolved at link time (board_pins.o is always linked via main).
extern int board_pins_i2s_bck(void);
extern int board_pins_i2s_lck(void);
extern int board_pins_i2s_data(void);

esp_err_t get_i2c_pins(i2c_port_t port, i2c_config_t *i2c_config)
{
    if (!i2c_config) return ESP_FAIL;

    i2c_config->sda_io_num = I2C_SDA;
    i2c_config->scl_io_num = I2C_SCL;

    return ESP_OK;
}

esp_err_t get_i2s_pins(int port, board_i2s_pin_t *i2s_config)
{
    if (!i2s_config) return ESP_FAIL;

    i2s_config->bck_io_num   = board_pins_i2s_bck();
    i2s_config->ws_io_num    = board_pins_i2s_lck();
    i2s_config->data_out_num = board_pins_i2s_data();
    i2s_config->data_in_num  = -1;
    i2s_config->mck_io_num   = -1;

    return ESP_OK;
}

int8_t get_pa_enable_gpio(void)
{
    return PA_ENABLE_GPIO;
}

int8_t get_es8311_mclk_src(void)
{
    return 0;
}

int8_t get_sdcard_open_file_num_max(void)
{
    return 5;
}

int8_t get_sdcard_intr_gpio(void)
{
    return -1;
}

int8_t get_sdcard_power_ctrl_gpio(void)
{
    return -1;
}

int8_t get_headphone_detect_gpio(void)
{
    return -1;
}