#include "xpt2046.h"

#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "XPT2046";

// Temporary HW-bring-up diagnostics: throttled dump of raw Z / X / Y so testers
// can report actual controller readings (this variant was never HW-calibrated).
// Set to 1 to re-enable while tuning TOUCH_RAW_* / XPT_Z_THRESHOLD.
#define XPT_DEBUG 0

// Control bytes: start bit set, 12-bit conversion, differential mode.
// A2..A0 select the channel (see datasheet table).
#define XPT_CMD_X    0xD0   // X position
#define XPT_CMD_Y    0x90   // Y position
#define XPT_CMD_Z1   0xB0   // Z1 (touch pressure)
#define XPT_CMD_Z2   0xC0   // Z2

#define XPT_Z_THRESHOLD  400   // measured Z below this → treat as "not pressed"
#define XPT_SAMPLES      4     // averaged per axis to cut ADC jitter

static spi_device_handle_t s_dev = NULL;

// One 12-bit conversion: a 3-byte full-duplex frame [cmd, 0, 0]. The result
// arrives MSB-first in bytes 1..2, left-aligned by 3 bits (so >> 3 → 12-bit).
static uint16_t xpt_xfer(uint8_t cmd)
{
    uint8_t tx[3] = { cmd, 0, 0 };
    uint8_t rx[3] = { 0 };
    spi_transaction_t t = {
        .length    = 3 * 8,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };
    if (spi_device_polling_transmit(s_dev, &t) != ESP_OK) return 0;
    return ((uint16_t)((rx[1] << 8) | rx[2])) >> 3;
}

void xpt2046_init(spi_host_device_t host, int cs_gpio)
{
    const spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 2 * 1000 * 1000,   // XPT2046 tops out ~2.5 MHz in 12-bit mode
        .mode           = 0,
        .spics_io_num   = cs_gpio,
        .queue_size     = 1,
    };

    esp_err_t err = spi_bus_add_device(host, &devcfg, &s_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_add_device failed: %s", esp_err_to_name(err));
        s_dev = NULL;
        return;
    }
    ESP_LOGI(TAG, "Initialized (CS=%d)", cs_gpio);
}

bool xpt2046_read(uint16_t *x, uint16_t *y)
{
    if (s_dev == NULL) return false;

    // Pressure first — skip the (noisy) coordinate reads when no finger is down.
    uint16_t z1 = xpt_xfer(XPT_CMD_Z1);
    uint16_t z2 = xpt_xfer(XPT_CMD_Z2);
    int z = (int)z1 + 4095 - (int)z2;

    // z1 == 0 is non-physical for a real press (a pressed panel always pulls z1
    // above 0). It means the controller isn't driving MISO — disconnected or a
    // dead read returning all-zeros, which the formula would otherwise turn into
    // z=4095 → a latched ghost touch at (0,0). Force "released" instead.
    if (z1 == 0) z = 0;

#if XPT_DEBUG
    static int64_t s_last_log_us = 0;
    int64_t now = esp_timer_get_time();
    bool log_now = (now - s_last_log_us) > 500000;   // throttle to ~2 Hz
#endif

    if (z < XPT_Z_THRESHOLD) {
#if XPT_DEBUG
        if (log_now) {
            s_last_log_us = now;
            ESP_LOGI(TAG, "z1=%u z2=%u z=%d (< thr %d) -> released",
                     z1, z2, z, XPT_Z_THRESHOLD);
        }
#endif
        return false;
    }

    uint32_t sx = 0, sy = 0;
    for (int i = 0; i < XPT_SAMPLES; i++) {
        sx += xpt_xfer(XPT_CMD_X);
        sy += xpt_xfer(XPT_CMD_Y);
    }
    uint16_t rx = (uint16_t)(sx / XPT_SAMPLES);
    uint16_t ry = (uint16_t)(sy / XPT_SAMPLES);

#if XPT_DEBUG
    if (log_now) {
        s_last_log_us = now;
        ESP_LOGI(TAG, "z1=%u z2=%u z=%d PRESSED  raw x=%u y=%u", z1, z2, z, rx, ry);
    }
#endif

    if (x) *x = rx;
    if (y) *y = ry;
    return true;
}
