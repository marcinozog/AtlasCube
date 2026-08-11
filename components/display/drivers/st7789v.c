#include "lvgl.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "trace.h"
#include "esp_heap_caps.h"
#include "defines.h"
#include "board_pins.h"
#include "ui_profile.h"
#include "settings.h"
#include "freertos/task.h"
#include <stdlib.h>

static const char *TAG = "ST7789V";

// 240x320 controller RAM, exposed in landscape as a logical 320x240 display.
// Full-size panels normally use 0,0. Keep these explicit because cropped
// ST7789V modules sometimes need a non-zero visible-window offset.
#define ST7789V_X_OFFSET 0
#define ST7789V_Y_OFFSET 0

// Two buffers let LVGL render while the previous SPI DMA transfer is active.
#define LVGL_BUF_LINES 20

#define LCD_BL_LEDC_TIMER    LEDC_TIMER_0
#define LCD_BL_LEDC_CHANNEL  LEDC_CHANNEL_0
#define LCD_BL_LEDC_FREQ_HZ  5000
#define LCD_BL_LEDC_RES      LEDC_TIMER_8_BIT

static void backlight_init(void);
void display_set_backlight(uint8_t brightness);
void display_set_invert(bool invert);
void display_set_bgr(bool bgr);
void display_set_flip(bool flip);

static spi_device_handle_t spi;

// The known-good ST7789V baseline uses INVON. The user setting reverses it.
static bool          s_invert_on    = false;
static volatile bool s_invert_dirty = false;

// Landscape baseline: MY + MV, RGB channel order. A 180-degree flip toggles
// MY + MX; the BGR bit (0x08) is the user's red/blue swap. Both live in the
// same MADCTL register, so one dirty flag covers them.
static bool          s_flip_on      = false;
static bool          s_swap_rb      = false;  // settings.display.bgr, XORed over the baseline
static volatile bool s_madctl_dirty = false;

static spi_transaction_t s_color_trans;
static volatile bool     s_color_inflight = false;

static uint8_t st7789v_madctl(void)
{
    uint8_t m = s_flip_on ? 0x60 : 0xA0;
    if (s_swap_rb) m ^= 0x08;
    return m;
}

// Every wait on the flush path is bounded, because LVGL's wait_for_flushing() is
// a bare `while(disp->flushing);` spin with no timeout: a flush_ready that never
// arrives hangs the LVGL task forever at prio 5 while audio keeps playing, and
// with ESP_TASK_WDT_PANIC off nothing resets the device. A lost completion must
// degrade to a dropped frame, never to that. One 20-line buffer at 40 MHz takes
// a few ms, so this is ample headroom over any legitimate transfer.
#define FLUSH_SPI_TIMEOUT_MS 200

// IRAM: CONFIG_SPI_MASTER_ISR_IN_IRAM=y, so this runs from an ISR that stays
// live while the flash cache is disabled (settings save, SPIFFS write, NVS
// commit). A flash-resident callback would fault there. lv_display_flush_ready()
// is pulled into IRAM too — see LV_ATTRIBUTE_FLUSH_READY in the root CMakeLists.
static void IRAM_ATTR spi_post_cb(spi_transaction_t *t)
{
    if (t->user) lv_display_flush_ready((lv_display_t *)t->user);
}

static void spi_init(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = g_pins.lcd_mosi,
        // MISO is used when an XPT2046 shares the LCD bus.
        .miso_io_num = g_pins.tp_miso,
        .sclk_io_num = g_pins.lcd_clk,
        .max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * 2 + 8,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(DISPLAY_HOST, &buscfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = DISPLAY_CLK_SPEED,
        .mode = 0,
        .spics_io_num = g_pins.lcd_cs,
        .queue_size = 7,
        .post_cb = spi_post_cb,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(DISPLAY_HOST, &devcfg, &spi));

    gpio_set_direction(g_pins.lcd_dc, GPIO_MODE_OUTPUT);
    gpio_set_direction(g_pins.lcd_rst, GPIO_MODE_OUTPUT);
}

static void lcd_cmd(uint8_t cmd)
{
    gpio_set_level(g_pins.lcd_dc, 0);
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd,
    };
    ESP_ERROR_CHECK(spi_device_transmit(spi, &t));
}

static void lcd_data(const void *data, size_t len)
{
    if (len == 0) return;
    gpio_set_level(g_pins.lcd_dc, 1);
    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = data,
    };
    ESP_ERROR_CHECK(spi_device_transmit(spi, &t));
}

static void st7789v_reset(void)
{
    gpio_set_level(g_pins.lcd_rst, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(g_pins.lcd_rst, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
}

static void st7789v_init_cmds(void)
{
    lcd_cmd(0x01); // SWRESET
    vTaskDelay(pdMS_TO_TICKS(150));

    lcd_cmd(0x11); // SLPOUT
    vTaskDelay(pdMS_TO_TICKS(120));

    s_flip_on = settings_get()->display.flip;
    s_swap_rb = settings_get()->display.bgr;
    uint8_t madctl = st7789v_madctl();
    lcd_cmd(0x36); lcd_data(&madctl, 1); // landscape 320x240

    const uint8_t colmod = 0x55; // RGB565
    lcd_cmd(0x3A); lcd_data(&colmod, 1);

    const uint8_t porch[] = {0x0C, 0x0C, 0x00, 0x33, 0x33};
    lcd_cmd(0xB2); lcd_data(porch, sizeof(porch));

    const uint8_t gate = 0x35;
    lcd_cmd(0xB7); lcd_data(&gate, 1);

    const uint8_t vcom = 0x19;
    lcd_cmd(0xBB); lcd_data(&vcom, 1);

    const uint8_t lcm = 0x2C;
    lcd_cmd(0xC0); lcd_data(&lcm, 1);

    const uint8_t vdv_vrh_en[] = {0x01, 0xFF};
    lcd_cmd(0xC2); lcd_data(vdv_vrh_en, sizeof(vdv_vrh_en));

    const uint8_t vrh = 0x12;
    lcd_cmd(0xC3); lcd_data(&vrh, 1);

    const uint8_t vdv = 0x20;
    lcd_cmd(0xC4); lcd_data(&vdv, 1);

    const uint8_t frame_rate = 0x0F;
    lcd_cmd(0xC6); lcd_data(&frame_rate, 1);

    const uint8_t power[] = {0xA4, 0xA1};
    lcd_cmd(0xD0); lcd_data(power, sizeof(power));

    const uint8_t gamma_pos[] = {
        0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F,
        0x54, 0x4C, 0x18, 0x0D, 0x0B, 0x1F, 0x23,
    };
    lcd_cmd(0xE0); lcd_data(gamma_pos, sizeof(gamma_pos));

    const uint8_t gamma_neg[] = {
        0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C, 0x3F,
        0x44, 0x51, 0x2F, 0x1F, 0x1F, 0x20, 0x23,
    };
    lcd_cmd(0xE1); lcd_data(gamma_neg, sizeof(gamma_neg));

    s_invert_on = settings_get()->display.invert;
    lcd_cmd(s_invert_on ? 0x20 : 0x21); // baseline INVON

    lcd_cmd(0x13); // NORON
    lcd_cmd(0x29); // DISPON
    vTaskDelay(pdMS_TO_TICKS(20));
}

static void set_address_window(int x1, int y1, int x2, int y2)
{
    x1 += ST7789V_X_OFFSET;
    x2 += ST7789V_X_OFFSET;
    y1 += ST7789V_Y_OFFSET;
    y2 += ST7789V_Y_OFFSET;

    uint8_t col[] = {x1 >> 8, x1 & 0xFF, x2 >> 8, x2 & 0xFF};
    lcd_cmd(0x2A); lcd_data(col, sizeof(col));

    uint8_t row[] = {y1 >> 8, y1 & 0xFF, y2 >> 8, y2 & 0xFF};
    lcd_cmd(0x2B); lcd_data(row, sizeof(row));

    lcd_cmd(0x2C);
}

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    if (s_color_inflight) {
        spi_transaction_t *finished;
        if (spi_device_get_trans_result(spi, &finished,
                                        pdMS_TO_TICKS(FLUSH_SPI_TIMEOUT_MS)) != ESP_OK) {
            // The descriptor is still owned by the driver, so s_color_trans must
            // not be reused — skip this frame, hand LVGL its flush_ready back so
            // the UI loop keeps running, and retry the reclaim on the next flush.
            ESP_LOGW(TAG, "flush reclaim timed out — frame dropped");
            lv_display_flush_ready(disp);
            return;
        }
        s_color_inflight = false;
    }

    if (s_invert_dirty) {
        s_invert_dirty = false;
        lcd_cmd(s_invert_on ? 0x20 : 0x21);
    }
    if (s_madctl_dirty) {
        s_madctl_dirty = false;
        uint8_t madctl = st7789v_madctl();
        lcd_cmd(0x36); lcd_data(&madctl, 1);
    }

    int width = area->x2 - area->x1 + 1;
    int height = area->y2 - area->y1 + 1;
    int pixel_count = width * height;

    // ~1 Hz sample of what the panel is actually being asked to repaint — the
    // reference point when a shared SPI bus starts misbehaving.
    TRACE_EVERY_MS(TRACE_DISPLAY, TAG, 1000, "flush x:%d-%d y:%d-%d (%d px)",
                   area->x1, area->x2, area->y1, area->y2, pixel_count);

    uint16_t *pixels = (uint16_t *)px_map;
    for (int i = 0; i < pixel_count; ++i) {
        pixels[i] = __builtin_bswap16(pixels[i]);
    }

    set_address_window(area->x1, area->y1, area->x2, area->y2);

    gpio_set_level(g_pins.lcd_dc, 1);
    s_color_trans = (spi_transaction_t){
        .length = (size_t)pixel_count * sizeof(uint16_t) * 8,
        .tx_buffer = px_map,
        .user = disp,
    };
    esp_err_t err = spi_device_queue_trans(spi, &s_color_trans,
                                           pdMS_TO_TICKS(FLUSH_SPI_TIMEOUT_MS));
    if (err != ESP_OK) {
        // Nothing was queued, so spi_post_cb will never fire for this frame —
        // without signalling here, LVGL spins on disp->flushing forever. This
        // used to be an ESP_ERROR_CHECK, i.e. abort + reboot mid-playback.
        ESP_LOGE(TAG, "queue_trans failed: %s — frame dropped", esp_err_to_name(err));
        lv_display_flush_ready(disp);
        return;
    }
    s_color_inflight = true;
    // spi_post_cb signals LVGL when the DMA transfer completes.
}

static void st7789v_clear(uint16_t color)
{
    set_address_window(0, 0, DISPLAY_WIDTH - 1, DISPLAY_HEIGHT - 1);

    static uint8_t line[DISPLAY_WIDTH * 2];
    for (int x = 0; x < DISPLAY_WIDTH; ++x) {
        line[x * 2] = color >> 8;
        line[x * 2 + 1] = color & 0xFF;
    }
    for (int y = 0; y < DISPLAY_HEIGHT; ++y) {
        lcd_data(line, sizeof(line));
    }
}

void st7789v_init(void)
{
#if DISPLAY_WIDTH != 320 || DISPLAY_HEIGHT != 240
#error "ST7789V driver currently requires UI_PROFILE_320x240"
#endif

    spi_init();
    st7789v_reset();
    st7789v_init_cmds();
    backlight_init();
    st7789v_clear(0x0000);

    lv_color_t *buf1 = heap_caps_malloc(
        DISPLAY_WIDTH * LVGL_BUF_LINES * sizeof(lv_color_t),
        MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    lv_color_t *buf2 = heap_caps_malloc(
        DISPLAY_WIDTH * LVGL_BUF_LINES * sizeof(lv_color_t),
        MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

    if (!buf1 || !buf2) {
        ESP_LOGE(TAG, "LVGL buffer allocation failed");
        free(buf1);
        free(buf2);
        return;
    }

    lv_display_t *disp = lv_display_create(DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_display_set_flush_cb(disp, flush_cb);
    lv_display_set_buffers(
        disp,
        buf1,
        buf2,
        DISPLAY_WIDTH * LVGL_BUF_LINES * sizeof(lv_color_t),
        LV_DISPLAY_RENDER_MODE_PARTIAL);

    ESP_LOGI(TAG, "ST7789V initialized (320x240 landscape, LVGL 9)");
}

static void backlight_init(void)
{
    if (g_pins.lcd_led < 0) {
        ESP_LOGI(TAG, "Backlight pin disabled (lcd_led < 0)");
        return;
    }

    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LCD_BL_LEDC_TIMER,
        .duty_resolution = LCD_BL_LEDC_RES,
        .freq_hz = LCD_BL_LEDC_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    ledc_channel_config_t channel = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LCD_BL_LEDC_CHANNEL,
        .timer_sel = LCD_BL_LEDC_TIMER,
        .gpio_num = g_pins.lcd_led,
        .duty = 255,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel));
}

void display_set_backlight(uint8_t brightness)
{
    if (g_pins.lcd_led < 0) return;
    if (brightness > 100) brightness = 100;
    uint32_t duty = (brightness * 255) / 100;
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LCD_BL_LEDC_CHANNEL, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LCD_BL_LEDC_CHANNEL));
}

void display_set_invert(bool invert)
{
    s_invert_on = invert;
    s_invert_dirty = true;
}

void display_set_bgr(bool bgr)
{
    s_swap_rb = bgr;
    s_madctl_dirty = true;
}

void display_set_flip(bool flip)
{
    s_flip_on = flip;
    s_madctl_dirty = true;
}
