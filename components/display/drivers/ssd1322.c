/*
 * ssd1322.c — SSD1322 256x64 4-bit grayscale OLED (4-wire SPI)
 *
 * Unlike the color TFT drivers in this folder, the SSD1322 is a 16-level
 * grayscale OLED:
 *   - self-emissive, so there is NO backlight pin; brightness is the
 *     contrast-current register (0xC1) and display_set_backlight() maps onto it.
 *   - GDDRAM packs two 4-bit pixels per byte (high nibble = left pixel).
 *   - one column address spans 4 horizontal pixels; a 256-wide panel is
 *     centered in the 480-wide GDDRAM, hence the 0x1C column offset.
 *
 * LVGL renders RGB565 (LV_COLOR_DEPTH=16). We use a full-screen render buffer
 * in PSRAM and convert the whole frame to packed 4-bit luma on each flush.
 * That sidesteps the 4-pixel column alignment that partial updates would
 * otherwise require, and keeps internal DRAM free for the ESP-ADF audio tasks
 * (see docs/display_drivers.md §3).
 */

#include "lvgl.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "defines.h"
#include "ui_profile.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "SSD1322";

// SSD1322 max SCLK is ~10 MHz (100 ns cycle). Keep it well below the shared
// DISPLAY_CLK_SPEED (40 MHz) the color TFT panels use.
#define SSD1322_CLK_SPEED   10000000

// A 256-wide panel sits centered in the 480-px-wide GDDRAM. Each column address
// covers 4 pixels, so the offset is (480-256)/2 / 4 = 28 = 0x1C. If the image is
// shifted horizontally, nudge this by ±1.
#define SSD1322_COL_OFFSET  0x1C

#define SSD1322_COLS        (DISPLAY_WIDTH / 4)                    // column addresses (4 px each)
#define SSD1322_GRAM_BYTES  (DISPLAY_WIDTH * DISPLAY_HEIGHT / 2)   // 2 px / byte

static spi_device_handle_t spi;
static uint8_t            *s_gram;   // packed 4-bit frame, DMA-capable

void display_set_backlight(uint8_t brightness);

/* =========================
   LOW LEVEL SPI
   ========================= */

static void spi_init(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = LCD_PIN_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = LCD_PIN_CLK,
        .max_transfer_sz = SSD1322_GRAM_BYTES + 8,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(DISPLAY_HOST, &buscfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = SSD1322_CLK_SPEED,
        .mode = 0,
        .spics_io_num = LCD_PIN_CS,
        .queue_size = 7,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(DISPLAY_HOST, &devcfg, &spi));

    gpio_set_direction(LCD_PIN_DC, GPIO_MODE_OUTPUT);
    gpio_set_direction(LCD_PIN_RST, GPIO_MODE_OUTPUT);
}

static void ssd1322_cmd(uint8_t cmd)
{
    gpio_set_level(LCD_PIN_DC, 0);
    spi_transaction_t t = { .length = 8, .tx_buffer = &cmd };
    spi_device_transmit(spi, &t);
}

static void ssd1322_data(const uint8_t *data, int len)
{
    if (len <= 0) return;
    gpio_set_level(LCD_PIN_DC, 1);
    spi_transaction_t t = { .length = len * 8, .tx_buffer = data };
    spi_device_transmit(spi, &t);
}

static void ssd1322_data1(uint8_t b) { ssd1322_data(&b, 1); }

/* =========================
   INIT
   ========================= */

static void ssd1322_reset(void)
{
    gpio_set_level(LCD_PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(LCD_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
}

static void ssd1322_init_cmds(void)
{
    ssd1322_cmd(0xFD); ssd1322_data1(0x12);                       // unlock command lock
    ssd1322_cmd(0xAE);                                            // display OFF

    ssd1322_cmd(0xB3); ssd1322_data1(0x91);                       // clock divide / osc freq
    ssd1322_cmd(0xCA); ssd1322_data1(0x3F);                       // multiplex ratio = 64
    ssd1322_cmd(0xA2); ssd1322_data1(0x00);                       // display offset
    ssd1322_cmd(0xA1); ssd1322_data1(0x00);                       // start line

    // Re-map: horizontal address increment, dual COM, nibble re-map.
    // Toggle bit 4 (0x10) of byte 0 to flip the image horizontally if mirrored.
    ssd1322_cmd(0xA0); ssd1322_data1(0x14); ssd1322_data1(0x11);

    ssd1322_cmd(0xB5); ssd1322_data1(0x00);                       // GPIO disabled
    ssd1322_cmd(0xAB); ssd1322_data1(0x01);                       // enable internal VDD regulator

    ssd1322_cmd(0xB4); ssd1322_data1(0xA0); ssd1322_data1(0xFD);  // display enhancement A
    ssd1322_cmd(0xC1); ssd1322_data1(0x9F);                       // contrast current
    ssd1322_cmd(0xC7); ssd1322_data1(0x0F);                       // master contrast
    ssd1322_cmd(0xB9);                                            // default linear grayscale table
    ssd1322_cmd(0xB1); ssd1322_data1(0xE2);                       // phase length
    ssd1322_cmd(0xD1); ssd1322_data1(0x82); ssd1322_data1(0x20);  // display enhancement B
    ssd1322_cmd(0xBB); ssd1322_data1(0x1F);                       // pre-charge voltage
    ssd1322_cmd(0xB6); ssd1322_data1(0x08);                       // second pre-charge period
    ssd1322_cmd(0xBE); ssd1322_data1(0x07);                       // VCOMH

    ssd1322_cmd(0xA6);                                            // normal display
    ssd1322_cmd(0xA9);                                            // exit partial display
}

static void ssd1322_set_window(void)
{
    ssd1322_cmd(0x15);                                            // set column address
    ssd1322_data1(SSD1322_COL_OFFSET);
    ssd1322_data1(SSD1322_COL_OFFSET + SSD1322_COLS - 1);
    ssd1322_cmd(0x75);                                            // set row address
    ssd1322_data1(0x00);
    ssd1322_data1(DISPLAY_HEIGHT - 1);
    ssd1322_cmd(0x5C);                                            // write RAM
}

/* =========================
   LVGL FLUSH (LVGL 9)
   ========================= */

static inline uint8_t rgb565_to_gray4(uint16_t px)
{
    uint8_t r = (px >> 11) & 0x1F;
    uint8_t g = (px >> 5)  & 0x3F;
    uint8_t b =  px        & 0x1F;
    uint8_t r8 = (r << 3) | (r >> 2);
    uint8_t g8 = (g << 2) | (g >> 4);
    uint8_t b8 = (b << 3) | (b >> 2);
    uint8_t luma = (r8 * 77 + g8 * 150 + b8 * 29) >> 8;  // Rec.601-ish weighting
    return luma >> 4;                                    // 0..15
}

static void my_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    // FULL render mode → area is the whole screen, px_map is the whole buffer.
    (void)area;
    const uint16_t *src = (const uint16_t *)px_map;
    int total = DISPLAY_WIDTH * DISPLAY_HEIGHT;
    for (int i = 0; i < total; i += 2) {
        uint8_t hi = rgb565_to_gray4(src[i]);       // left pixel  → high nibble
        uint8_t lo = rgb565_to_gray4(src[i + 1]);   // right pixel → low nibble
        s_gram[i >> 1] = (hi << 4) | lo;
    }

    ssd1322_set_window();
    ssd1322_data(s_gram, SSD1322_GRAM_BYTES);

    lv_display_flush_ready(disp);
}

/* =========================
   PUBLIC INIT
   ========================= */

void ssd1322_init(void)
{
    spi_init();
    ssd1322_reset();
    ssd1322_init_cmds();

    // Packed 4-bit frame pushed to the panel — must be DMA-capable.
    s_gram = heap_caps_malloc(SSD1322_GRAM_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!s_gram) {
        ESP_LOGE(TAG, "GRAM alloc failed");
        return;
    }

    // Blank panel RAM before turning the display on.
    memset(s_gram, 0, SSD1322_GRAM_BYTES);
    ssd1322_set_window();
    ssd1322_data(s_gram, SSD1322_GRAM_BYTES);

    ssd1322_cmd(0xAF);   // display ON

    // Full-screen LVGL render buffer in PSRAM (256*64*2 = 32 KB RGB565).
    // FULL mode keeps internal DRAM free for ESP-ADF (see display_drivers.md §3).
    static lv_color_t *buf;
    buf = heap_caps_malloc(DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(lv_color_t),
                           MALLOC_CAP_SPIRAM);
    if (!buf) {
        ESP_LOGE(TAG, "LVGL buffer alloc failed");
        return;
    }

    lv_display_t *disp = lv_display_create(DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_display_set_flush_cb(disp, my_flush_cb);
    lv_display_set_buffers(
        disp,
        buf,
        NULL,
        DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(lv_color_t),
        LV_DISPLAY_RENDER_MODE_FULL
    );

    ESP_LOGI(TAG, "SSD1322 256x64 initialized (LVGL 9)");
}

/* =========================
   BRIGHTNESS (contrast — no backlight on OLED)
   ========================= */

/**
 * @brief Set OLED brightness via the contrast-current register.
 * @param brightness  0 = off, 100 = full brightness
 */
void display_set_backlight(uint8_t brightness)
{
    if (brightness > 100) brightness = 100;
    ssd1322_cmd(0xC1);
    ssd1322_data1((brightness * 255) / 100);
}
