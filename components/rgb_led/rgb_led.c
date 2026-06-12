#include "rgb_led.h"
#include "defines.h"

#include "esp_log.h"
#include "led_strip.h"

static const char *TAG = "RGB_LED";

// Single on-board pixel.
#define RGB_LED_COUNT          1
// 10 MHz RMT resolution → 100 ns ticks, fine for WS2812 timing requirements
// (1.25 us bit period). Matches the example in led_strip docs.
#define RGB_LED_RMT_RES_HZ     (10 * 1000 * 1000)

static led_strip_handle_t s_strip   = NULL;
static uint8_t            s_bright  = 100;  // 0..100, applied per-set
static bool               s_inited  = false;

static inline uint8_t scale8(uint8_t v, uint8_t pct)
{
    // Integer 0..100 scaling, rounded to nearest.
    return (uint8_t)(((uint32_t)v * pct + 50) / 100);
}

esp_err_t rgb_led_init(void)
{
    if (s_inited) return ESP_OK;

    if (RGB_LED_PIN < 0) {
        ESP_LOGI(TAG, "RGB LED disabled on this board (RGB_LED_PIN=%d)", RGB_LED_PIN);
        return ESP_ERR_NOT_SUPPORTED;
    }

    led_strip_config_t strip_cfg = {
        .strip_gpio_num   = RGB_LED_PIN,
        .max_leds         = RGB_LED_COUNT,
        .led_model        = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = {
            .invert_out = false,
        },
    };

    led_strip_rmt_config_t rmt_cfg = {
        .clk_src        = RMT_CLK_SRC_DEFAULT,
        .resolution_hz  = RGB_LED_RMT_RES_HZ,
        .mem_block_symbols = 64,
        .flags = {
            .with_dma = false,   // single LED — DMA is overkill
        },
    };

    esp_err_t err = led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &s_strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "led_strip_new_rmt_device failed: %s", esp_err_to_name(err));
        s_strip = NULL;
        return err;
    }

    // Start with LED off — avoids whatever the WS2812 latch happens to be
    // showing after power-on / reset.
    led_strip_clear(s_strip);

    s_inited = true;
    ESP_LOGI(TAG, "Initialized on GPIO %d", RGB_LED_PIN);
    return ESP_OK;
}

void rgb_led_set(uint8_t r, uint8_t g, uint8_t b)
{
    if (!s_inited || !s_strip) return;

    uint8_t br = s_bright;
    if (br > 100) br = 100;

    uint8_t rr = scale8(r, br);
    uint8_t gg = scale8(g, br);
    uint8_t bb = scale8(b, br);

    esp_err_t err = led_strip_set_pixel(s_strip, 0, rr, gg, bb);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "set_pixel failed: %s", esp_err_to_name(err));
        return;
    }
    led_strip_refresh(s_strip);
}

void rgb_led_off(void)
{
    if (!s_inited || !s_strip) return;
    led_strip_clear(s_strip);
}

void rgb_led_set_brightness(uint8_t pct)
{
    if (pct > 100) pct = 100;
    s_bright = pct;
}

bool rgb_led_is_available(void)
{
    return s_inited;
}
