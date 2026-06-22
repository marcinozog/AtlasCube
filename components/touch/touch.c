#include "touch.h"
#include "defines.h"
#include "ui_profile.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "lvgl.h"

#if CONFIG_TOUCH_CST816D
#include "cst816d.h"
#endif
#if CONFIG_TOUCH_FT6336U
#include "ft6336u.h"
#endif

static const char *TAG = "TOUCH";

static i2c_master_bus_handle_t s_bus = NULL;
static lv_indev_t *s_indev = NULL;
static volatile bool s_int_flag = false;
static bool s_pressed = false;

static void IRAM_ATTR touch_isr(void *arg)
{
    s_int_flag = true;
}

static bool touch_driver_read(uint16_t *x, uint16_t *y)
{
#if CONFIG_TOUCH_CST816D
    return cst816d_read(x, y);
#elif CONFIG_TOUCH_FT6336U
    return ft6336u_read(x, y);
#else
    (void)x; (void)y;
    return false;
#endif
}

static void touch_lvgl_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    // INT line idles high. When low (or recently fell), poll the controller.
    // We always poll on PRESSED so LVGL keeps receiving move events while
    // the finger is held — the chip only re-asserts INT on state changes.
    bool int_low = (CTP_INT >= 0) ? (gpio_get_level(CTP_INT) == 0) : true;
    bool poll    = s_int_flag || int_low || s_pressed;
    s_int_flag = false;

    if (!poll) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    uint16_t x = 0, y = 0;
    if (touch_driver_read(&x, &y)) {
#if TOUCH_SWAP_XY
        { uint16_t t = x; x = y; y = t; }
#endif
#if TOUCH_MIRROR_X
        x = DISPLAY_WIDTH  - 1 - x;
#endif
#if TOUCH_MIRROR_Y
        y = DISPLAY_HEIGHT - 1 - y;
#endif
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
        s_pressed = true;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
        s_pressed = false;
    }
}

void touch_init(void)
{
#if CONFIG_TOUCH_NONE
    ESP_LOGI(TAG, "Touch disabled (TOUCH_NONE)");
    return;
#elif (CTP_SCL < 0) || (CTP_SDA < 0)
    ESP_LOGW(TAG, "Touch I2C pins not configured (SCL=%d SDA=%d) — skipped",
             CTP_SCL, CTP_SDA);
    return;
#else
    // ── RST pin (optional) ───────────────────────────────────────────────
    if (CTP_RST >= 0) {
        gpio_config_t io = {
            .pin_bit_mask = (1ULL << CTP_RST),
            .mode         = GPIO_MODE_OUTPUT,
            .pull_up_en   = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type    = GPIO_INTR_DISABLE,
        };
        gpio_config(&io);
        gpio_set_level(CTP_RST, 1);
    }

    // ── I2C master bus ───────────────────────────────────────────────────
    const i2c_master_bus_config_t bus_cfg = {
        .clk_source                   = I2C_CLK_SRC_DEFAULT,
        .i2c_port                     = I2C_NUM_0,
        .scl_io_num                   = CTP_SCL,
        .sda_io_num                   = CTP_SDA,
        .glitch_ignore_cnt            = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t err = i2c_new_master_bus(&bus_cfg, &s_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_new_master_bus failed: %s", esp_err_to_name(err));
        return;
    }

    // ── Driver ───────────────────────────────────────────────────────────
#if CONFIG_TOUCH_CST816D
    cst816d_init(s_bus, CTP_RST);
#elif CONFIG_TOUCH_FT6336U
    ft6336u_init(s_bus, CTP_RST);
#else
    ESP_LOGE(TAG, "No touch driver selected in Kconfig");
    return;
#endif

    // ── INT pin (optional, falling-edge wake-up for the LVGL read_cb) ────
    if (CTP_INT >= 0) {
        gpio_config_t io = {
            .pin_bit_mask = (1ULL << CTP_INT),
            .mode         = GPIO_MODE_INPUT,
            .pull_up_en   = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type    = GPIO_INTR_NEGEDGE,
        };
        gpio_config(&io);

        // gpio_install_isr_service may already be installed (e.g. by encoder)
        esp_err_t ret = gpio_install_isr_service(0);
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "gpio_install_isr_service failed: %s", esp_err_to_name(ret));
        }
        gpio_isr_handler_add(CTP_INT, touch_isr, NULL);
    }

    // ── LVGL pointer indev ───────────────────────────────────────────────
    s_indev = lv_indev_create();
    lv_indev_set_type(s_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(s_indev, touch_lvgl_read_cb);

    ESP_LOGI(TAG, "Initialized — SCL=%d SDA=%d INT=%d RST=%d",
             CTP_SCL, CTP_SDA, CTP_INT, CTP_RST);
#endif
}
