#include "touch.h"
#include "defines.h"
#include "board_pins.h"
#include "ui_profile.h"
#include "settings.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "trace.h"
#include "lvgl.h"

#if CONFIG_TOUCH_CST816D
#include "cst816d.h"
#endif
#if CONFIG_TOUCH_FT6336U
#include "ft6336u.h"
#endif
#if CONFIG_TOUCH_XPT2046
#include "xpt2046.h"
#include "driver/spi_master.h"
#endif

static const char *TAG = "TOUCH";

#if CONFIG_TOUCH_CST816D || CONFIG_TOUCH_FT6336U
static i2c_master_bus_handle_t s_bus = NULL;
#endif
static lv_indev_t *s_indev = NULL;
static volatile bool s_int_flag = false;
static bool s_pressed = false;
static int  s_irq_gpio = -1;   // INT/PENIRQ line that gates polling (-1 = polled)

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
#elif CONFIG_TOUCH_XPT2046
    return xpt2046_read(x, y);
#else
    (void)x; (void)y;
    return false;
#endif
}

#if CONFIG_TOUCH_XPT2046
// Map a raw 12-bit ADC reading to a pixel coordinate. Direction is encoded in
// the calibration range itself: pass min < max for a normal axis, or min > max
// to mirror it (so TOUCH_MIRROR_* are not needed for resistive panels).
static inline uint16_t raw_to_px(uint16_t raw, int rmin, int rmax, int span)
{
    int den = rmax - rmin;
    int v   = (den != 0) ? ((int)raw - rmin) * (span - 1) / den : 0;
    if (v < 0)        v = 0;
    if (v > span - 1) v = span - 1;
    return (uint16_t)v;
}
#endif

static void touch_lvgl_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    // INT line idles high. When low (or recently fell), poll the controller.
    // We always poll on PRESSED so LVGL keeps receiving move events while
    // the finger is held — the chip only re-asserts INT on state changes.
    bool int_low = (s_irq_gpio >= 0) ? (gpio_get_level(s_irq_gpio) == 0) : true;
    bool poll    = s_int_flag || int_low || s_pressed;
    s_int_flag = false;

    if (!poll) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    uint16_t x = 0, y = 0;
    if (touch_driver_read(&x, &y)) {
        // Orientation = per-profile baseline (ui_profile.h) XOR the runtime
        // overrides from settings, so a user whose digitizer is mounted
        // differently can fix it from the web UI without a rebuild. Deliberately
        // NOT tied to display.flip: rotating the image does not rotate the
        // touch layer glued to the panel.
        const display_settings_t *d = &settings_get()->display;
        const bool swap_xy  = (bool)TOUCH_SWAP_XY ^ d->touch_swap_xy;
#if CONFIG_TOUCH_XPT2046
        // Resistive: axis direction is baked into the calibration ranges (MIN >
        // MAX mirrors an axis), so TOUCH_MIRROR_* stays out of this baseline.
        const bool invert_x = d->touch_invert_x;
        const bool invert_y = d->touch_invert_y;
#else
        const bool invert_x = (bool)TOUCH_MIRROR_X ^ d->touch_invert_x;
        const bool invert_y = (bool)TOUCH_MIRROR_Y ^ d->touch_invert_y;
#endif

        // Swap first, while the reading is still in the controller's own space:
        // for the resistive path TOUCH_RAW_X_* bounds whichever raw channel
        // ends up feeding screen X.
        if (swap_xy) { uint16_t t = x; x = y; y = t; }

#if CONFIG_TOUCH_XPT2046
        // Resistive controller returns raw ADC — scale each axis to the screen.
        x = raw_to_px(x, TOUCH_RAW_X_MIN, TOUCH_RAW_X_MAX, DISPLAY_WIDTH);
        y = raw_to_px(y, TOUCH_RAW_Y_MIN, TOUCH_RAW_Y_MAX, DISPLAY_HEIGHT);
#endif
        // Both paths are in pixel space by now (capacitive controllers report
        // pixels straight away), so the mirrors are the same either way.
        if (invert_x) x = DISPLAY_WIDTH  - 1 - x;
        if (invert_y) y = DISPLAY_HEIGHT - 1 - y;

        // The other half of the calibration story: the driver's trace prints the
        // raw reading, this one the pixel it lands on after swap/scale/mirror.
        TRACE_EVERY_MS(TRACE_TOUCH, TAG, 500,
                       "press -> x=%u y=%u (swap=%d invx=%d invy=%d)",
                       x, y, swap_xy, invert_x, invert_y);

        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
        s_pressed = true;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
        s_pressed = false;
    }
}

// ── INT/PENIRQ pin (optional, falling-edge wake-up for the LVGL read_cb) ──────
static void touch_setup_irq(void)
{
    if (s_irq_gpio < 0) return;

    gpio_config_t io = {
        .pin_bit_mask = (1ULL << s_irq_gpio),
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
    gpio_isr_handler_add(s_irq_gpio, touch_isr, NULL);
}

static void touch_register_indev(void)
{
    s_indev = lv_indev_create();
    lv_indev_set_type(s_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(s_indev, touch_lvgl_read_cb);
}

#if CONFIG_TOUCH_XPT2046
static void touch_init_xpt2046(void)
{
    if (g_pins.tp_cs < 0) {
        ESP_LOGW(TAG, "XPT2046 CS pin not configured — skipped");
        return;
    }

    spi_host_device_t host;
    if (g_pins.tp_clk >= 0 && g_pins.tp_mosi >= 0) {
        // Dedicated SPI3 bus.
        host = SPI3_HOST;
        const spi_bus_config_t buscfg = {
            .mosi_io_num     = g_pins.tp_mosi,
            .miso_io_num     = g_pins.tp_miso,
            .sclk_io_num     = g_pins.tp_clk,
            .quadwp_io_num   = -1,
            .quadhd_io_num   = -1,
            .max_transfer_sz = 0,
        };
        esp_err_t err = spi_bus_initialize(host, &buscfg, SPI_DMA_DISABLED);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "spi_bus_initialize(SPI3) failed: %s", esp_err_to_name(err));
            return;
        }
        ESP_LOGI(TAG, "XPT2046 on dedicated SPI3 (CLK=%d MOSI=%d MISO=%d)",
                 g_pins.tp_clk, g_pins.tp_mosi, g_pins.tp_miso);
    } else {
        // Share the LCD bus — display_init() already brought it up (with MISO
        // wired to tp_miso). XPT2046 just adds a second device with its own CS.
        host = DISPLAY_HOST;
        ESP_LOGI(TAG, "XPT2046 shares the LCD SPI bus (MISO=%d)", g_pins.tp_miso);
    }

    xpt2046_init(host, g_pins.tp_cs);

    s_irq_gpio = g_pins.tp_irq;
    touch_setup_irq();
    touch_register_indev();

    ESP_LOGI(TAG, "Initialized (XPT2046) — CS=%d IRQ=%d", g_pins.tp_cs, g_pins.tp_irq);
}
#endif

#if CONFIG_TOUCH_CST816D || CONFIG_TOUCH_FT6336U
static void touch_init_i2c(void)
{
    // Pins are runtime now (TOUCH_NONE stays compile-time — it's a driver choice).
    if (g_pins.ctp_scl < 0 || g_pins.ctp_sda < 0) {
        ESP_LOGW(TAG, "Touch I2C pins not configured (SCL=%d SDA=%d) — skipped",
                 g_pins.ctp_scl, g_pins.ctp_sda);
        return;
    }

    // ── RST pin (optional) ───────────────────────────────────────────────
    if (g_pins.ctp_rst >= 0) {
        gpio_config_t io = {
            .pin_bit_mask = (1ULL << g_pins.ctp_rst),
            .mode         = GPIO_MODE_OUTPUT,
            .pull_up_en   = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type    = GPIO_INTR_DISABLE,
        };
        gpio_config(&io);
        gpio_set_level(g_pins.ctp_rst, 1);
    }

    // ── I2C master bus ───────────────────────────────────────────────────
    const i2c_master_bus_config_t bus_cfg = {
        .clk_source                   = I2C_CLK_SRC_DEFAULT,
        .i2c_port                     = I2C_NUM_0,
        .scl_io_num                   = g_pins.ctp_scl,
        .sda_io_num                   = g_pins.ctp_sda,
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
    cst816d_init(s_bus, g_pins.ctp_rst);
#elif CONFIG_TOUCH_FT6336U
    ft6336u_init(s_bus, g_pins.ctp_rst);
#endif

    s_irq_gpio = g_pins.ctp_int;
    touch_setup_irq();
    touch_register_indev();

    ESP_LOGI(TAG, "Initialized — SCL=%d SDA=%d INT=%d RST=%d",
             g_pins.ctp_scl, g_pins.ctp_sda, g_pins.ctp_int, g_pins.ctp_rst);
}
#endif

void touch_init(void)
{
#if CONFIG_TOUCH_NONE
    ESP_LOGI(TAG, "Touch disabled (TOUCH_NONE)");
    return;
#elif CONFIG_TOUCH_XPT2046
    touch_init_xpt2046();
#elif CONFIG_TOUCH_CST816D || CONFIG_TOUCH_FT6336U
    touch_init_i2c();
#else
    ESP_LOGE(TAG, "No touch driver selected in Kconfig");
#endif
}
