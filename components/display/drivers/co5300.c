#include "defines.h"
#include <string.h>
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "lvgl.h"

static const char *TAG = "CO5300";

#define QSPI_WRITE_CMD      0x32

// === MIPI DCS commands ===
#define CMD_SWRESET         0x01
#define CMD_SLPOUT          0x11
#define CMD_NORON           0x13
#define CMD_INVOFF          0x20
#define CMD_INVON           0x21
#define CMD_DISPOFF         0x28
#define CMD_DISPON          0x29
#define CMD_CASET           0x2A
#define CMD_RASET           0x2B
#define CMD_RAMWR           0x2C
#define CMD_TEON            0x35
#define CMD_MADCTL          0x36
#define CMD_COLMOD          0x3A
#define CMD_WRDISBV         0x51
#define CMD_WRCTRLD         0x53
#define CMD_WRHBMDISBV      0x63
#define CMD_SPIM            0xC4
#define CMD_PGSW            0xFE

#define CS_LOW()    gpio_set_level(DISPLAY_PIN_CS, 0)
#define CS_HIGH()   gpio_set_level(DISPLAY_PIN_CS, 1)

#define LVGL_BUF_LINES 40

spi_device_handle_t display_spi;
spi_transaction_ext_t spi_tran_ext;
spi_transaction_t *spi_tran;

// Recursive — flush_cb / fillRect take the lock around a full
// CASET+RASET+RAMWR+bulk sequence and the inner writers re-take it.
static SemaphoreHandle_t s_spi_mtx;

#define SPI_LOCK()    xSemaphoreTakeRecursive(s_spi_mtx, portMAX_DELAY)
#define SPI_UNLOCK()  xSemaphoreGiveRecursive(s_spi_mtx)

static inline uint16_t even(uint16_t v) { return v & ~1; }

void writeCommand(uint8_t cmd)
{
    SPI_LOCK();
    CS_LOW();
    spi_tran_ext.base.flags = SPI_TRANS_MULTILINE_CMD | SPI_TRANS_MULTILINE_ADDR;
    spi_tran_ext.base.cmd = 0x02;
    spi_tran_ext.base.addr = ((uint32_t)cmd) << 8;
    spi_tran_ext.base.tx_buffer = NULL;
    spi_tran_ext.base.length = 0;
    spi_device_transmit(display_spi, spi_tran);
    CS_HIGH();
    SPI_UNLOCK();
}

void writeC8D8(uint8_t c, uint8_t d)
{
    SPI_LOCK();
    CS_LOW();
    spi_tran_ext.base.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_MULTILINE_CMD | SPI_TRANS_MULTILINE_ADDR;
    spi_tran_ext.base.cmd = 0x02;
    spi_tran_ext.base.addr = ((uint32_t)c) << 8;
    spi_tran_ext.base.tx_data[0] = d;
    spi_tran_ext.base.length = 8;
    spi_device_transmit(display_spi, spi_tran);
    CS_HIGH();
    SPI_UNLOCK();
}

void writeC8D16D16(uint8_t c, uint16_t d1, uint16_t d2)
{
    SPI_LOCK();
    CS_LOW();
    spi_tran_ext.base.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_MULTILINE_CMD | SPI_TRANS_MULTILINE_ADDR;
    spi_tran_ext.base.cmd = 0x02;
    spi_tran_ext.base.addr = ((uint32_t)c) << 8;
    spi_tran_ext.base.tx_data[0] = d1 >> 8;
    spi_tran_ext.base.tx_data[1] = d1 & 0xFF;
    spi_tran_ext.base.tx_data[2] = d2 >> 8;
    spi_tran_ext.base.tx_data[3] = d2 & 0xFF;
    spi_tran_ext.base.length = 32;
    spi_device_transmit(display_spi, spi_tran);
    CS_HIGH();
    SPI_UNLOCK();
}

void write16(uint16_t d)
{
    SPI_LOCK();
    CS_LOW();
    spi_tran_ext.base.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_MODE_QIO;
    spi_tran_ext.base.cmd = QSPI_WRITE_CMD;
    spi_tran_ext.base.addr = 0x003C00;
    spi_tran_ext.base.tx_data[0] = d >> 8;
    spi_tran_ext.base.tx_data[1] = d;
    spi_tran_ext.base.length = 16;
    spi_device_transmit(display_spi, spi_tran);
    CS_HIGH();
    SPI_UNLOCK();
}

void display_hw_reset(void)
{
    gpio_set_level(DISPLAY_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(DISPLAY_PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(DISPLAY_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(200));
}

void setAddrWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    x = even(x);
    y = even(y);
    w = even(w);
    h = even(h);

    uint16_t x0 = x;
    uint16_t y0 = y;

    writeC8D16D16(CMD_CASET, x0, x0 + w - 1);
    writeC8D16D16(CMD_RASET, y0, y0 + h - 1);
    writeCommand(CMD_RAMWR);
}

void fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    x = even(x);
    y = even(y);
    w = even(w);
    h = even(h);

    if (x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT) return;

    uint32_t pixel_count = (uint32_t)w * h;

    uint32_t buffer_size = pixel_count * 2;
    uint8_t *pixel_data = heap_caps_malloc(buffer_size, MALLOC_CAP_DMA);
    if (pixel_data == NULL) {
        ESP_LOGE(TAG, "Failed to allocate pixel buffer");
        return;
    }

    uint8_t color_hi = color >> 8;
    uint8_t color_lo = color & 0xFF;
    for (uint32_t i = 0; i < pixel_count; i++) {
        pixel_data[i * 2] = color_hi;
        pixel_data[i * 2 + 1] = color_lo;
    }

    SPI_LOCK();
    setAddrWindow(x, y, w, h);

    CS_LOW();
    spi_tran_ext.base.flags = SPI_TRANS_MODE_QIO;
    spi_tran_ext.base.cmd = QSPI_WRITE_CMD;
    spi_tran_ext.base.addr = (CMD_RAMWR << 8) | 0x00;
    spi_tran_ext.base.tx_buffer = pixel_data;
    spi_tran_ext.base.length = buffer_size * 8;
    spi_device_transmit(display_spi, spi_tran);
    CS_HIGH();
    SPI_UNLOCK();

    heap_caps_free(pixel_data);
}

esp_err_t display_spi_init(void)
{
    esp_err_t ret;

    gpio_config_t cs_cfg = {
        .pin_bit_mask = (1ULL << DISPLAY_PIN_CS),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&cs_cfg);
    CS_HIGH();

    gpio_config_t rst_cfg = {
        .pin_bit_mask = (1ULL << DISPLAY_PIN_RST),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&rst_cfg);
    gpio_set_level(DISPLAY_PIN_RST, 1);

    spi_bus_config_t buscfg = {
        .mosi_io_num = DISPLAY_PIN_D0,
        .miso_io_num = DISPLAY_PIN_D1,
        .sclk_io_num = DISPLAY_PIN_CLK,
        .quadwp_io_num = DISPLAY_PIN_D2,
        .quadhd_io_num = DISPLAY_PIN_D3,
        .data4_io_num = -1,
        .data5_io_num = -1,
        .data6_io_num = -1,
        .data7_io_num = -1,
        .max_transfer_sz = (DISPLAY_WIDTH * DISPLAY_HEIGHT * 2) + 64,
        .flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_GPIO_PINS,
        .intr_flags = 0,
        .isr_cpu_id = ESP_INTR_CPU_AFFINITY_AUTO,
    };

    ret = spi_bus_initialize(DISPLAY_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus init failed");
        return ret;
    }

    spi_device_interface_config_t devcfg = {
        .command_bits = 8,
        .address_bits = 24,
        .dummy_bits = 0,
        .mode = 0,
        .clock_speed_hz = DISPLAY_CLK_SPEED,
        .spics_io_num = -1,
        .flags = SPI_DEVICE_HALFDUPLEX,
        .queue_size = 4,
        .duty_cycle_pos = 0,
        .cs_ena_pretrans = 0,
        .cs_ena_posttrans = 0,
        .input_delay_ns = 0,
    };

    ret = spi_bus_add_device(DISPLAY_HOST, &devcfg, &display_spi);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Device add failed");
        return ret;
    }

    // Bus is intentionally not acquired here — display_spi_init runs in
    // app_main, but flush_cb runs in lvgl_task on the other core. Polling
    // SPI APIs are tied to the acquiring task, so an acquire here would
    // make cross-task QSPI writes silently misbehave (only the first band
    // of every LVGL frame lands on the panel). spi_device_transmit (queued
    // mode) is thread-safe instead.

    memset(&spi_tran_ext, 0, sizeof(spi_tran_ext));
    spi_tran = (spi_transaction_t *)&spi_tran_ext;

    s_spi_mtx = xSemaphoreCreateRecursiveMutex();
    if (!s_spi_mtx) {
        ESP_LOGE(TAG, "SPI mutex create failed");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "SPI initialized (panel %dx%d)", DISPLAY_WIDTH, DISPLAY_HEIGHT);

    display_hw_reset();
    return ESP_OK;
}

esp_err_t display_panel_init(void)
{
    ESP_LOGI(TAG, "Init sequence...");

    writeC8D8(CMD_PGSW,        0x00);   // 0xFE - vendor command page
    writeC8D8(CMD_SPIM,        0x80);   // 0xC4 - SPI mode / SRAM write enable
    writeC8D8(CMD_TEON,        0x00);   // 0x35 - TE on, V-blank only
    writeC8D8(CMD_COLMOD,      0x55);   // 0x3A - RGB565
    writeC8D8(CMD_WRCTRLD,     0x20);   // 0x53 - brightness ctrl block on
    writeC8D8(CMD_WRDISBV,     0xE0);   // 0x51 - display brightness
    writeC8D8(CMD_WRHBMDISBV,  0xE0);   // 0x63 - HBM brightness

    writeC8D8(CMD_MADCTL,      0xC0);   // 0x36 - Orientation

    writeCommand(CMD_SLPOUT);
    vTaskDelay(pdMS_TO_TICKS(80));

    writeCommand(CMD_DISPON);
    vTaskDelay(pdMS_TO_TICKS(50));

    ESP_LOGI(TAG, "Init complete");
    return ESP_OK;
}

static void clear_panel(uint16_t color)
{
    // Full panel buffer wouldn't fit in DMA RAM; stripe-fill in 32-line bands.
    const uint16_t band = 32;
    for (uint16_t y = 0; y < DISPLAY_HEIGHT; y += band) {
        uint16_t h = (y + band <= DISPLAY_HEIGHT) ? band : (DISPLAY_HEIGHT - y);
        fillRect(0, y, DISPLAY_WIDTH, h, color);
    }
}

// CO5300 requires even CASET/RASET boundaries; Y is rounded to even via this
// hook. X is forced to the full display width on every refresh: with partial
// X ranges and an odd original x1 the rounder's leftmost added column shows
// up as a black artifact (LVGL's SW rasterizer leaves it unpainted, the DMA
// buffer starts zeroed). Full-width refresh sidesteps the issue and on a
// 240-wide panel the bandwidth cost is negligible.
static void co5300_rounder_cb(lv_event_t *e)
{
    lv_area_t *a = lv_event_get_param(e);
    a->x1 = 0;
    a->x2 = DISPLAY_WIDTH - 1;
    a->y1 &= ~1;
    a->y2 |= 1;
}

static void co5300_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    int x1 = area->x1, x2 = area->x2;
    int y1 = area->y1, y2 = area->y2;
    int w = x2 - x1 + 1;
    int h = y2 - y1 + 1;
    int pixels = w * h;

    // ESP_LOGD(TAG, "Flush: x:%d-%d, y:%d-%d, w:%d", area->x1, area->x2, area->y1, area->y2, w);

    // RGB565 LE → BE in place. The buffer was allocated MALLOC_CAP_DMA so
    // it's fine to hand directly to the QSPI bulk write.
    uint16_t *buf = (uint16_t *)px_map;
    for (int i = 0; i < pixels; i++) {
        buf[i] = __builtin_bswap16(buf[i]);
    }

    SPI_LOCK();
    writeC8D16D16(CMD_CASET, x1, x1 + w - 1);
    writeC8D16D16(CMD_RASET, y1, y1 + h - 1);
    writeCommand(CMD_RAMWR);

    CS_LOW();
    spi_tran_ext.base.flags = SPI_TRANS_MODE_QIO;
    spi_tran_ext.base.cmd = QSPI_WRITE_CMD;
    spi_tran_ext.base.addr = (CMD_RAMWR << 8) | 0x00;
    spi_tran_ext.base.tx_buffer = px_map;
    spi_tran_ext.base.length = (uint32_t)pixels * 16;
    spi_tran_ext.base.rxlength = 0;
    spi_device_transmit(display_spi, spi_tran);
    CS_HIGH();
    SPI_UNLOCK();

    lv_display_flush_ready(disp);
}

void co5300_init(void)
{
    if (display_spi_init() != ESP_OK) {
        ESP_LOGE(TAG, "SPI init failed");
        return;
    }
    else
        ESP_LOGI(TAG, "SPI init ok");

    if (display_panel_init() != ESP_OK) {
        ESP_LOGE(TAG, "Panel init failed");
        return;
    }
    else
        ESP_LOGI(TAG, "Panel init OK");

    // GRAM boots with random data on AMOLEDs — clear the whole panel so the
    // area outside the centered UI window stays black.
    clear_panel(0x0000);

    static lv_color_t *buf = NULL;
    buf = heap_caps_malloc(DISPLAY_WIDTH * LVGL_BUF_LINES * sizeof(lv_color_t),
                           MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!buf) {
        ESP_LOGE(TAG, "LVGL buffer alloc failed");
        return;
    }

    lv_display_t *disp = lv_display_create(DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_display_set_flush_cb(disp, co5300_flush_cb);
    lv_display_add_event_cb(disp, co5300_rounder_cb, LV_EVENT_INVALIDATE_AREA, NULL);
    lv_display_set_buffers(disp, buf, NULL,
                           DISPLAY_WIDTH * LVGL_BUF_LINES,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    // lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_90);

    ESP_LOGI(TAG, "CO5300 initialized — %dx%d", DISPLAY_WIDTH, DISPLAY_HEIGHT);
}

void display_set_backlight(uint8_t brightness)
{
    if (brightness > 100) brightness = 100;
    uint8_t dcs = (uint8_t)((brightness * 0xFF) / 100);
    writeC8D8(CMD_WRDISBV, dcs);
}