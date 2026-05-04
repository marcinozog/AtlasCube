#include "lvgl.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "defines.h"
#include "freertos/task.h"

static const char *TAG = "ILI9341";

#define LVGL_BUF_LINES 40

#define LCD_BL_LEDC_TIMER    LEDC_TIMER_0
#define LCD_BL_LEDC_CHANNEL  LEDC_CHANNEL_0
#define LCD_BL_LEDC_FREQ_HZ  5000
#define LCD_BL_LEDC_RES      LEDC_TIMER_8_BIT   // 0–255

static void backlight_init(void);
void ili9341_set_backlight(uint8_t brightness);

static spi_device_handle_t spi;

/* =========================
   LOW LEVEL SPI
   ========================= */

static void spi_init(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = LCD_PIN_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = LCD_PIN_CLK,
        .max_transfer_sz = LCD_WIDTH * LCD_HEIGHT * 2 + 8
    };

    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 40 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = LCD_PIN_CS,
        .queue_size = 7,
    };

    ESP_ERROR_CHECK(spi_bus_add_device(LCD_HOST, &devcfg, &spi));

    gpio_set_direction(LCD_PIN_DC, GPIO_MODE_OUTPUT);
    gpio_set_direction(LCD_PIN_RST, GPIO_MODE_OUTPUT);
}

/* =========================
   LCD COMMANDS
   ========================= */

static void lcd_cmd(uint8_t cmd)
{
    gpio_set_level(LCD_PIN_DC, 0);

    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd
    };

    spi_device_transmit(spi, &t);
}

static void lcd_data(const uint8_t *data, int len)
{
    gpio_set_level(LCD_PIN_DC, 1);

    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = data
    };

    spi_device_transmit(spi, &t);
}

/* =========================
   INIT
   ========================= */

static void ili9341_reset(void)
{
    gpio_set_level(LCD_PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(LCD_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
}

static void ili9341_init_cmds(void)
{
    lcd_cmd(0x01); // SW reset
    vTaskDelay(pdMS_TO_TICKS(150));

    lcd_cmd(0x11); // sleep out
    vTaskDelay(pdMS_TO_TICKS(120));

    lcd_cmd(0xCF);
    uint8_t d1[] = {0x00, 0xC1, 0x30};
    lcd_data(d1, 3);

    lcd_cmd(0xED);
    uint8_t d2[] = {0x64, 0x03, 0x12, 0x81};
    lcd_data(d2, 4);

    lcd_cmd(0xE8);
    uint8_t d3[] = {0x85, 0x00, 0x78};
    lcd_data(d3, 3);

    lcd_cmd(0xCB);
    uint8_t d4[] = {0x39, 0x2C, 0x00, 0x34, 0x02};
    lcd_data(d4, 5);

    lcd_cmd(0xF7);
    uint8_t d5[] = {0x20};
    lcd_data(d5, 1);

    lcd_cmd(0xEA);
    uint8_t d6[] = {0x00, 0x00};
    lcd_data(d6, 2);

    lcd_cmd(0xC0); // Power control
    uint8_t d7[] = {0x23};
    lcd_data(d7, 1);

    lcd_cmd(0xC1); // Power control
    uint8_t d8[] = {0x10};
    lcd_data(d8, 1);

    lcd_cmd(0xC5); // VCOM control
    uint8_t d9[] = {0x3E, 0x28};
    lcd_data(d9, 2);

    lcd_cmd(0xC7); // VCOM control 2
    uint8_t d10[] = {0x86};
    lcd_data(d10, 1);

    lcd_cmd(0x36); // MADCTL
    uint8_t d11[] = {0xE8}; //0x20, 0x40, 0x80, 0xE0 | 0x08
    lcd_data(d11, 1);

    lcd_cmd(0x3A); // Pixel format
    uint8_t d12[] = {0x55}; // RGB565
    lcd_data(d12, 1);

    lcd_cmd(0xB1); // Frame rate
    uint8_t d13[] = {0x00, 0x18};
    lcd_data(d13, 2);

    lcd_cmd(0xB6); // Display function control
    uint8_t d14[] = {0x08, 0x82, 0x27};
    lcd_data(d14, 3);

    lcd_cmd(0xF2); // 3Gamma disable
    uint8_t d15[] = {0x00};
    lcd_data(d15, 1);

    lcd_cmd(0x26); // Gamma curve
    uint8_t d16[] = {0x01};
    lcd_data(d16, 1);

    lcd_cmd(0xE0); // Positive gamma
    uint8_t d17[] = {0x0F,0x31,0x2B,0x0C,0x0E,0x08,0x4E,0xF1,
                     0x37,0x07,0x10,0x03,0x0E,0x09,0x00};
    lcd_data(d17, 15);

    lcd_cmd(0xE1); // Negative gamma
    uint8_t d18[] = {0x00,0x0E,0x14,0x03,0x11,0x07,0x31,0xC1,
                     0x48,0x08,0x0F,0x0C,0x31,0x36,0x0F};
    lcd_data(d18, 15);

    lcd_cmd(0x21); // INVON — compensates the panel's default state
    lcd_cmd(0x29); // Display ON
    vTaskDelay(pdMS_TO_TICKS(20));
}

/* =========================
   LVGL FLUSH (LVGL 9)
   ========================= */

static void my_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    int x1 = area->x1, x2 = area->x2;
    int y1 = area->y1, y2 = area->y2;
    int size = (x2 - x1 + 1) * (y2 - y1 + 1);

    uint16_t *buf = (uint16_t *)px_map;
    for (int i = 0; i < size; i++) {
        buf[i] = __builtin_bswap16(buf[i]);
    }

    uint8_t col_data[4] = { x1>>8, x1&0xFF, x2>>8, x2&0xFF };
    lcd_cmd(0x2A); lcd_data(col_data, 4);

    uint8_t row_data[4] = { y1>>8, y1&0xFF, y2>>8, y2&0xFF };
    lcd_cmd(0x2B); lcd_data(row_data, 4);

    lcd_cmd(0x2C);
    lcd_data(px_map, size * 2);

    lv_display_flush_ready(disp);
}

static void ili9341_clear(uint16_t color)
{
    uint8_t col[4] = {0x00, 0x00, (319 >> 8), (319 & 0xFF)};
    uint8_t row[4] = {0x00, 0x00, (239 >> 8), (239 & 0xFF)};
    lcd_cmd(0x2A); lcd_data(col, 4);
    lcd_cmd(0x2B); lcd_data(row, 4);
    lcd_cmd(0x2C);

    // single buffer instead of a loop
    static uint8_t line[320 * 2];
    uint8_t hi = color >> 8;
    uint8_t lo = color & 0xFF;
    for (int i = 0; i < 320; i++) {
        line[i*2]   = hi;
        line[i*2+1] = lo;
    }
    for (int y = 0; y < 240; y++) {
        lcd_data(line, sizeof(line));
    }
}

/* =========================
   PUBLIC INIT
   ========================= */

void ili9341_init(void)
{
    spi_init();
    ili9341_reset();
    ili9341_init_cmds();
    backlight_init();
    ili9341_clear(0x0000);

    /* LVGL buffer */
    static lv_color_t *buf = NULL;

    buf = heap_caps_malloc(LCD_WIDTH * LVGL_BUF_LINES * sizeof(lv_color_t),
                           MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

    if (!buf) {
        ESP_LOGE(TAG, "Buffer alloc failed");
        return;
    }

    /* LVGL display */
    lv_display_t *disp = lv_display_create(LCD_WIDTH, LCD_HEIGHT);
    // lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565_SWAP);
    lv_display_set_flush_cb(disp, my_flush_cb);

    lv_display_set_buffers(
        disp,
        buf,
        NULL,
        LCD_WIDTH * LVGL_BUF_LINES,
        LV_DISPLAY_RENDER_MODE_PARTIAL
    );

    ESP_LOGI(TAG, "ILI9341 initialized (LVGL 9)");
}

/* =========================
   BACKLIGHT (PWM)
   ========================= */

static void backlight_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .timer_num       = LCD_BL_LEDC_TIMER,
        .duty_resolution = LCD_BL_LEDC_RES,
        .freq_hz         = LCD_BL_LEDC_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    ledc_channel_config_t channel = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LCD_BL_LEDC_CHANNEL,
        .timer_sel  = LCD_BL_LEDC_TIMER,
        .gpio_num   = LCD_LED,
        .duty       = 255,   // full brightness on start
        .hpoint     = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel));
}

/**
 * @brief Set the LCD backlight brightness.
 * @param brightness  0 = off, 100 = full brightness
 */
void ili9341_set_backlight(uint8_t brightness)
{
    if (brightness > 100) brightness = 100;
    uint32_t duty = (brightness * 255) / 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LCD_BL_LEDC_CHANNEL, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LCD_BL_LEDC_CHANNEL);
}