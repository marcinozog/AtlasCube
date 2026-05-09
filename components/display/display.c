/*
 * display.c — hardware init + start of lvgl_task
 *
 * Changes vs the old version:
 *  - ui_init() → ui_manager_init() (before task start)
 *  - while(1){lv_timer_handler()} → ui_manager_run() (blocks)
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "display.h"
#include "board_def.h"
#include "esp_log.h"
#include "lvgl.h"
#include "esp_timer.h"
#include "ui_manager.h"

static const char *TAG = "DISPLAY";

void lvgl_tick_init(void);
static void lvgl_task(void *arg);

void display_init(void)
{
    lv_init();
    lvgl_tick_init();

#if CONFIG_DISPLAY_ILI9341
    extern void ili9341_init(void);
    ili9341_init();
#elif CONFIG_DISPLAY_CO5300
    extern void co5300_init(void);
    co5300_init();
#elif CONFIG_DISPLAY_ST7796
    extern void st7796_init(void);
    st7796_init();
#elif CONFIG_DISPLAY_SSD1322
    extern void ssd1322_init(void);
    ssd1322_init();
#else
    #error "Unknown DISPLAY_TYPE"
#endif

    // Initialize the manager BEFORE starting the task (creates queue, registers callback)
    ui_manager_init();

    xTaskCreatePinnedToCore(
        lvgl_task,
        "lvgl",
        16384,
        NULL,
        5,
        NULL,
        1   // CPU1
    );

    ESP_LOGI(TAG, "Display initialized");
}

static void lvgl_task(void *arg)
{
    // ui_manager_run() creates the startup screen and runs the LVGL loop + event queue.
    // This function never returns.
    ui_manager_run();
}

static void lv_tick_task(void *arg)
{
    lv_tick_inc(1);
}

void lvgl_tick_init(void)
{
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &lv_tick_task,
        .name = "lv_tick"
    };

    esp_timer_handle_t timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer, 1000));
}