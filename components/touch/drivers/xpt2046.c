#include "xpt2046.h"

#include "esp_log.h"
#include "trace.h"
#include "freertos/FreeRTOS.h"
#include <string.h>

static const char *TAG = "XPT2046";

// Raw Z / X / Y dumps live behind the runtime "touch" trace flag (web UI:
// Settings → Diagnostics → Diagnostic logging): this variant was never
// HW-calibrated, so a tester on a stock release binary has to be able to report
// actual controller readings without rebuilding.

// Control bytes: start bit set, 12-bit conversion, differential mode.
// A2..A0 select the channel (see datasheet table).
#define XPT_CMD_X    0xD0   // X position
#define XPT_CMD_Y    0x90   // Y position
#define XPT_CMD_Z1   0xB0   // Z1 (touch pressure)
#define XPT_CMD_Z2   0xC0   // Z2

#define XPT_Z_THRESHOLD  400   // measured Z below this → treat as "not pressed"
#define XPT_SAMPLES      4     // averaged per axis to cut ADC jitter

// Every wait below is bounded, and deliberately so. This controller normally
// shares SPI2 with the panel, whose flush queues an interrupt-driven DMA
// transfer and returns before it completes — so a conversion routinely starts
// while the LCD still owns the bus. spi_device_polling_transmit() cannot survive
// that: it forces portMAX_DELAY on both of its halves (the bus-lock handover in
// spi_device_polling_start, and the bare `while (!spi_hal_usr_is_done())` spin in
// spi_device_polling_end), and it takes the bus by acquisition, which suspends
// the interrupt-driven transactions of the other device. One lost handover then
// hangs the LVGL task forever — no panic, no reset, radio still playing.
// Queued transactions take a real timeout and use the same arbitration path as
// the panel, so both devices on the bus now go through one mechanism.
#define XPT_SPI_TIMEOUT_MS 50

static spi_device_handle_t s_dev = NULL;

// Transfer buffers for one conversion. Four bytes and static rather than three
// on the stack, because when the XPT2046 shares the LCD bus that bus has DMA
// enabled, and GDMA on the S3 needs an RX buffer that is word-aligned *and* a
// multiple of 4 bytes long (GDMA_LL_AHB_RX_BURST_NEEDS_ALIGNMENT). A 3-byte
// stack buffer misses both, so IDF silently falls back to a
// heap_caps_aligned_alloc(DMA|INTERNAL) + free on *every* conversion — 2 per
// idle poll, 10 per touched poll, at up to 200 polls/s. That is constant churn
// on the scarcest heap in this firmware. Static .bss is DMA-capable whatever
// happens to task stack placement; safe as static because conversions only ever
// run from the LVGL indev read_cb, i.e. one task.
static WORD_ALIGNED_ATTR uint8_t s_tx[4];
static WORD_ALIGNED_ATTR uint8_t s_rx[4];

// The descriptor is static for the same reason, plus one of its own: when a
// conversion times out the driver still owns it, so it has to outlive the call.
static spi_transaction_t s_trans;
static bool s_trans_pending = false;   // driver still owns s_trans / s_rx
static bool s_spi_failed    = false;   // one log line per failure streak

// read_cb runs on every LVGL tick, so an unconditional log would bury the
// console. One line when the bus starts failing and one when it recovers is
// enough to see it in a user's log.
static void xpt_report_spi(const char *stage, esp_err_t err)
{
    if (s_spi_failed) return;
    s_spi_failed = true;
    ESP_LOGW(TAG, "SPI %s failed: %s — touch samples dropped",
             stage, esp_err_to_name(err));
}

// One 12-bit conversion: a 4-byte full-duplex frame [cmd, 0, 0, 0]. The result
// arrives MSB-first in bytes 1..2, left-aligned by 3 bits (so >> 3 → 12-bit);
// the 4th byte is padding the controller shifts out as zeros, which costs 8
// extra clocks at 2 MHz and changes nothing about the reading.
static uint16_t xpt_xfer(uint8_t cmd)
{
    spi_transaction_t *finished = NULL;
    esp_err_t err;

    // A conversion that timed out earlier left the descriptor and s_rx with the
    // driver. Reclaim them before reuse, or drop this sample and retry later.
    if (s_trans_pending) {
        if (spi_device_get_trans_result(s_dev, &finished,
                                        pdMS_TO_TICKS(XPT_SPI_TIMEOUT_MS)) != ESP_OK)
            return 0;
        s_trans_pending = false;
    }

    memset(s_tx, 0, sizeof(s_tx));
    memset(s_rx, 0, sizeof(s_rx));
    s_tx[0] = cmd;

    s_trans = (spi_transaction_t){
        .length    = sizeof(s_tx) * 8,
        .rxlength  = sizeof(s_rx) * 8,
        .tx_buffer = s_tx,
        .rx_buffer = s_rx,
    };

    err = spi_device_queue_trans(s_dev, &s_trans, pdMS_TO_TICKS(XPT_SPI_TIMEOUT_MS));
    if (err != ESP_OK) {
        xpt_report_spi("queue", err);
        return 0;
    }
    s_trans_pending = true;

    err = spi_device_get_trans_result(s_dev, &finished,
                                      pdMS_TO_TICKS(XPT_SPI_TIMEOUT_MS));
    if (err != ESP_OK) {
        xpt_report_spi("result", err);
        return 0;              // stays pending — the next call retries the reclaim
    }
    s_trans_pending = false;

    if (s_spi_failed) {
        ESP_LOGI(TAG, "SPI recovered");
        s_spi_failed = false;
    }
    return ((uint16_t)((s_rx[1] << 8) | s_rx[2])) >> 3;
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

    if (z < XPT_Z_THRESHOLD) {
        // ~2 Hz: this branch runs on every LVGL poll, so an unthrottled line
        // would be a wall of text. A z that wanders instead of sitting near 0
        // is the signature of a noisy or contended MISO.
        TRACE_EVERY_MS(TRACE_TOUCH, TAG, 500,
                       "z1=%u z2=%u z=%d (< thr %d) -> released",
                       z1, z2, z, XPT_Z_THRESHOLD);
        return false;
    }

    uint32_t sx = 0, sy = 0;
    for (int i = 0; i < XPT_SAMPLES; i++) {
        sx += xpt_xfer(XPT_CMD_X);
        sy += xpt_xfer(XPT_CMD_Y);
    }
    uint16_t rx = (uint16_t)(sx / XPT_SAMPLES);
    uint16_t ry = (uint16_t)(sy / XPT_SAMPLES);

    // Raw corner readings from a real finger are what TOUCH_RAW_* is tuned to.
    TRACE_EVERY_MS(TRACE_TOUCH, TAG, 500,
                   "z1=%u z2=%u z=%d PRESSED  raw x=%u y=%u", z1, z2, z, rx, ry);

    if (x) *x = rx;
    if (y) *y = ry;
    return true;
}
