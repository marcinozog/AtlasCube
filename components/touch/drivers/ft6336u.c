#include "ft6336u.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "FT6336U";

#define FT6336U_I2C_ADDR        0x38
#define FT6336U_I2C_FREQ_HZ     400000
#define FT6336U_REG_TD_STATUS   0x02    // td_status, then P1_XH, XL, YH, YL

static i2c_master_dev_handle_t s_dev = NULL;

static void ft6336u_reset(int rst_gpio)
{
    if (rst_gpio < 0) return;

    gpio_set_level(rst_gpio, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(rst_gpio, 1);
    vTaskDelay(pdMS_TO_TICKS(200));
}

void ft6336u_init(i2c_master_bus_handle_t bus, int rst_gpio)
{
    if (!bus) {
        ESP_LOGE(TAG, "no I2C bus handle");
        return;
    }

    ft6336u_reset(rst_gpio);

    esp_err_t err = i2c_master_probe(bus, FT6336U_I2C_ADDR, 100);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "no I2C ACK at 0x%02X (RST floating? wiring?): %s",
                 FT6336U_I2C_ADDR, esp_err_to_name(err));
        return;
    }

    const i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = FT6336U_I2C_ADDR,
        .scl_speed_hz    = FT6336U_I2C_FREQ_HZ,
    };

    err = i2c_master_bus_add_device(bus, &dev_cfg, &s_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_master_bus_add_device failed: %s", esp_err_to_name(err));
        s_dev = NULL;
        return;
    }

    ESP_LOGI(TAG, "Initialized at 0x%02X", FT6336U_I2C_ADDR);
}

bool ft6336u_read(uint16_t *x, uint16_t *y)
{
    if (s_dev == NULL) return false;

    uint8_t reg = FT6336U_REG_TD_STATUS;
    uint8_t buf[5] = {0};

    esp_err_t err = i2c_master_transmit_receive(s_dev, &reg, 1, buf, sizeof(buf), 50);
    if (err != ESP_OK) return false;

    // buf[0] = td_status (low 4 bits = num points), buf[1..4] = xH, xL, yH, yL
    if ((buf[0] & 0x0F) == 0) return false;

    uint16_t rx = ((uint16_t)(buf[1] & 0x0F) << 8) | buf[2];
    uint16_t ry = ((uint16_t)(buf[3] & 0x0F) << 8) | buf[4];

    if (x) *x = rx;
    if (y) *y = ry;
    return true;
}
