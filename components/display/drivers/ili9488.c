#include "lvgl.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "defines.h"
#include "board_pins.h"
#include "ui_profile.h"
#include "settings.h"
#include "freertos/task.h"

static const char *TAG = "ILI9488";

#define LVGL_BUF_LINES 20

#define LCD_BL_LEDC_TIMER    LEDC_TIMER_0
#define LCD_BL_LEDC_CHANNEL  LEDC_CHANNEL_0
#define LCD_BL_LEDC_FREQ_HZ  5000
#define LCD_BL_LEDC_RES      LEDC_TIMER_8_BIT   // 0–255

static void backlight_init(void);
void display_set_backlight(uint8_t brightness);
void display_set_invert(bool invert);
void display_set_flip(bool flip);

static spi_device_handle_t spi;

// Colour-inversion state. Baseline is INVOFF (0x20); the `invert` flag XORs it.
// display_set_invert() (any task) latches state + a dirty flag; my_flush_cb()
// (LVGL task, owner of all SPI commands) sends the DCS command.
static bool          s_invert_on    = false;
static volatile bool s_invert_dirty = false;

// 180° flip state — same live mechanism as colour inversion. Baseline MADCTL is
// 0xE8; flip toggles MY+MX → 0x28. Touch follows settings.display.flip at runtime.
static bool          s_flip_on      = false;
static volatile bool s_flip_dirty   = false;

/* =========================
   LOW LEVEL SPI
   ========================= */

static void spi_init(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = g_pins.lcd_mosi,
        .miso_io_num = -1,
        .sclk_io_num = g_pins.lcd_clk,
        // 18-bit RGB666 over SPI → 3 bytes/pixel. We flush row-by-row, so the
        // largest single transfer is one display row (DISPLAY_WIDTH * 3 bytes).
        .max_transfer_sz = DISPLAY_WIDTH * 3 + 8
    };

    ESP_ERROR_CHECK(spi_bus_initialize(DISPLAY_HOST, &buscfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = DISPLAY_CLK_SPEED,
        .mode = 0,
        .spics_io_num = g_pins.lcd_cs,
        .queue_size = 7,
    };

    ESP_ERROR_CHECK(spi_bus_add_device(DISPLAY_HOST, &devcfg, &spi));

    gpio_set_direction(g_pins.lcd_dc, GPIO_MODE_OUTPUT);
    gpio_set_direction(g_pins.lcd_rst, GPIO_MODE_OUTPUT);
}

/* =========================
   LCD COMMANDS
   ========================= */

static void lcd_cmd(uint8_t cmd)
{
    gpio_set_level(g_pins.lcd_dc, 0);

    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd
    };

    spi_device_transmit(spi, &t);
}

static void lcd_data(const uint8_t *data, int len)
{
    gpio_set_level(g_pins.lcd_dc, 1);

    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = data
    };

    spi_device_transmit(spi, &t);
}

/* =========================
   INIT
   ========================= */

static void ili9488_reset(void)
{
    gpio_set_level(g_pins.lcd_rst, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(g_pins.lcd_rst, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
}

static void ili9488_init_cmds(void)
{
    lcd_cmd(0x01); // SW reset
    vTaskDelay(pdMS_TO_TICKS(120));

    lcd_cmd(0xE0); // PGAMCTRL — positive gamma
    uint8_t d1[] = {0x00, 0x03, 0x09, 0x08, 0x16, 0x0A, 0x3F, 0x78,
                    0x4C, 0x09, 0x0A, 0x08, 0x16, 0x1A, 0x0F};
    lcd_data(d1, 15);

    lcd_cmd(0xE1); // NGAMCTRL — negative gamma
    uint8_t d2[] = {0x00, 0x16, 0x19, 0x03, 0x0F, 0x05, 0x32, 0x45,
                    0x46, 0x04, 0x0E, 0x0D, 0x35, 0x37, 0x0F};
    lcd_data(d2, 15);

    lcd_cmd(0xC0); // Power control 1
    uint8_t d3[] = {0x17, 0x15};
    lcd_data(d3, 2);

    lcd_cmd(0xC1); // Power control 2
    uint8_t d4[] = {0x41};
    lcd_data(d4, 1);

    lcd_cmd(0xC5); // VCOM control
    uint8_t d5[] = {0x00, 0x12, 0x80};
    lcd_data(d5, 3);

    lcd_cmd(0x36); // MADCTL — landscape 480x320
    // 0xE8 = MY + MX + MV (row/col exchange) + BGR, mirroring the ST7796 profile.
    // UNTESTED on real hardware — flip the BGR bit (0x08) if red/blue are swapped,
    // or toggle MX/MY (0x40/0x80) if the image is mirrored/upside-down.
    // Flip 180° toggles MY+MX (0xC0) → 0x28.
    s_flip_on = settings_get()->display.flip;
    uint8_t d6[] = { s_flip_on ? (uint8_t)0x28 : (uint8_t)0xE8 };
    lcd_data(d6, 1);

    lcd_cmd(0x3A); // Pixel format
    // ILI9488 in 4-wire SPI mode supports ONLY 18-bit (RGB666) or 3-bit — it
    // cannot do 16-bit RGB565 like ST7796/ILI9341. So 0x66, and the flush
    // callback expands every RGB565 pixel into 3 bytes.
    uint8_t d7[] = {0x66};
    lcd_data(d7, 1);

    lcd_cmd(0xB0); // Interface mode control
    uint8_t d8[] = {0x00};
    lcd_data(d8, 1);

    lcd_cmd(0xB1); // Frame rate control
    uint8_t d9[] = {0xA0};
    lcd_data(d9, 1);

    lcd_cmd(0xB4); // Display inversion control
    uint8_t d10[] = {0x02};
    lcd_data(d10, 1);

    lcd_cmd(0xB6); // Display function control
    uint8_t d11[] = {0x02, 0x02};
    lcd_data(d11, 2);

    lcd_cmd(0xE9); // Set image function — disable 24-bit data bus
    uint8_t d12[] = {0x00};
    lcd_data(d12, 1);

    lcd_cmd(0xF7); // Adjust control 3
    uint8_t d13[] = {0xA9, 0x51, 0x2C, 0x82};
    lcd_data(d13, 4);

    lcd_cmd(0x11); // Sleep out
    vTaskDelay(pdMS_TO_TICKS(120));

    // INVOFF (0x20) is this panel's known-good baseline; settings.display.invert
    // flips to INVON (0x21) for batches that come out colour-inverted.
    s_invert_on = settings_get()->display.invert;
    lcd_cmd(s_invert_on ? 0x21 : 0x20);

    lcd_cmd(0x29); // Display ON
    vTaskDelay(pdMS_TO_TICKS(20));
}

/* =========================
   LVGL FLUSH (LVGL 9)
   ========================= */

// Expand one RGB565 pixel (native LE in the LVGL buffer) into 3 bytes of
// RGB666; the panel reads the top 6 bits of each byte.
static inline void rgb565_to_rgb666(uint16_t px, uint8_t *out)
{
    out[0] = (px & 0xF800) >> 8;  // R: 5 bits -> top of byte
    out[1] = (px & 0x07E0) >> 3;  // G: 6 bits -> top of byte
    out[2] = (px & 0x001F) << 3;  // B: 5 bits -> top of byte
}

static void my_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    if (s_invert_dirty) {            // apply a pending colour-inversion toggle
        s_invert_dirty = false;
        lcd_cmd(s_invert_on ? 0x21 : 0x20);
    }
    if (s_flip_dirty) {              // apply a pending 180° flip
        s_flip_dirty = false;
        uint8_t m = s_flip_on ? 0x28 : 0xE8;
        lcd_cmd(0x36); lcd_data(&m, 1);
    }

    int x1 = area->x1, x2 = area->x2;
    int y1 = area->y1, y2 = area->y2;
    int w = x2 - x1 + 1;
    int h = y2 - y1 + 1;

    uint8_t col_data[4] = { x1>>8, x1&0xFF, x2>>8, x2&0xFF };
    lcd_cmd(0x2A); lcd_data(col_data, 4);

    uint8_t row_data[4] = { y1>>8, y1&0xFF, y2>>8, y2&0xFF };
    lcd_cmd(0x2B); lcd_data(row_data, 4);

    lcd_cmd(0x2C);

    // Convert and push one row at a time. Keeps the DMA-capable scratch buffer
    // tiny (DISPLAY_WIDTH * 3 ≈ 1.4 kB) instead of a full-frame 18-bit buffer,
    // which would blow the internal-DRAM budget (see feedback on LVGL buffers).
    static uint8_t linebuf[DISPLAY_WIDTH * 3];
    uint16_t *src = (uint16_t *)px_map;
    for (int r = 0; r < h; r++) {
        uint8_t *d = linebuf;
        for (int c = 0; c < w; c++) {
            rgb565_to_rgb666(*src++, d);
            d += 3;
        }
        lcd_data(linebuf, w * 3);
    }

    lv_display_flush_ready(disp);
}

static void ili9488_clear(uint16_t color)
{
    uint8_t col[4] = {0x00, 0x00, ((DISPLAY_WIDTH - 1) >> 8), ((DISPLAY_WIDTH - 1) & 0xFF)};
    uint8_t row[4] = {0x00, 0x00, ((DISPLAY_HEIGHT - 1) >> 8), ((DISPLAY_HEIGHT - 1) & 0xFF)};
    lcd_cmd(0x2A); lcd_data(col, 4);
    lcd_cmd(0x2B); lcd_data(row, 4);
    lcd_cmd(0x2C);

    // single 18-bit line buffer instead of a loop
    static uint8_t line[DISPLAY_WIDTH * 3];
    uint8_t px[3];
    rgb565_to_rgb666(color, px);
    for (int i = 0; i < DISPLAY_WIDTH; i++) {
        line[i*3]   = px[0];
        line[i*3+1] = px[1];
        line[i*3+2] = px[2];
    }
    for (int y = 0; y < DISPLAY_HEIGHT; y++) {
        lcd_data(line, sizeof(line));
    }
}

/* =========================
   PUBLIC INIT
   ========================= */

void ili9488_init(void)
{
    spi_init();
    ili9488_reset();
    ili9488_init_cmds();
    backlight_init();
    ili9488_clear(0x0000);

    /* LVGL buffer — still RGB565 (2 B/px); conversion to RGB666 happens in flush. */
    static lv_color_t *buf = NULL;

    buf = heap_caps_malloc(DISPLAY_WIDTH * LVGL_BUF_LINES * sizeof(lv_color_t),
                           MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

    if (!buf) {
        ESP_LOGE(TAG, "Buffer alloc failed");
        return;
    }

    /* LVGL display */
    lv_display_t *disp = lv_display_create(DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_display_set_flush_cb(disp, my_flush_cb);

    lv_display_set_buffers(
        disp,
        buf,
        NULL,
        DISPLAY_WIDTH * LVGL_BUF_LINES,
        LV_DISPLAY_RENDER_MODE_PARTIAL
    );

    ESP_LOGI(TAG, "ILI9488 initialized (LVGL 9, 18-bit SPI)");
}

/* =========================
   BACKLIGHT (PWM)
   ========================= */

static void backlight_init(void)
{
    if (g_pins.lcd_led < 0) {
        ESP_LOGI(TAG, "Backlight pin disabled (lcd_led < 0) — no PWM");
        return;
    }
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
        .gpio_num   = g_pins.lcd_led,
        .duty       = 255,   // full brightness on start
        .hpoint     = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel));
}

/**
 * @brief Set the LCD backlight brightness.
 * @param brightness  0 = off, 100 = full brightness
 */
void display_set_backlight(uint8_t brightness)
{
    if (g_pins.lcd_led < 0) return;
    if (brightness > 100) brightness = 100;
    uint32_t duty = (brightness * 255) / 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LCD_BL_LEDC_CHANNEL, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LCD_BL_LEDC_CHANNEL);
}

void display_set_invert(bool invert)
{
    s_invert_on    = invert;
    s_invert_dirty = true;   // sent on the next flush, from the LVGL task
}

void display_set_flip(bool flip)
{
    s_flip_on    = flip;
    s_flip_dirty = true;     // sent on the next flush, from the LVGL task
}
