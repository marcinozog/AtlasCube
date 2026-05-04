#include "board.h"
#include "board_pins_config.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "BOARD_PINS";

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

    i2s_config->bck_io_num   = I2S_BCK;
    i2s_config->ws_io_num    = I2S_LCK;
    i2s_config->data_out_num = I2S_DATA;
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