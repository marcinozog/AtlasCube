#include "buzzer.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char *TAG = "BUZZER";

// NOTE: timer 0 / channel 0 are used by the LCD backlight (ili9341.c).
// Using the same timer resets its frequency and would turn off the display.
#define BUZZER_LEDC_MODE      LEDC_LOW_SPEED_MODE
#define BUZZER_LEDC_TIMER     LEDC_TIMER_1
#define BUZZER_LEDC_CHANNEL   LEDC_CHANNEL_1
#define BUZZER_LEDC_RES       LEDC_TIMER_10_BIT
#define BUZZER_DEFAULT_FREQ   2000
#define BUZZER_DUTY_ON        512      // 50% of 2^10
#define BUZZER_QUEUE_LEN      32

typedef struct {
    uint32_t freq_hz;
    uint32_t duration_ms;
} tone_cmd_t;

static QueueHandle_t s_queue;
static TaskHandle_t  s_task;
static bool          s_initialized;

static void buzzer_output_on(uint32_t freq_hz)
{
    ledc_set_freq(BUZZER_LEDC_MODE, BUZZER_LEDC_TIMER, freq_hz);
    ledc_set_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL, BUZZER_DUTY_ON);
    ledc_update_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL);
}

static void buzzer_output_off(void)
{
    ledc_set_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL, 0);
    ledc_update_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL);
}

static void buzzer_task(void *arg)
{
    (void)arg;
    tone_cmd_t cmd;
    for (;;) {
        if (xQueueReceive(s_queue, &cmd, portMAX_DELAY) != pdTRUE) continue;
        if (cmd.freq_hz > 0) {
            buzzer_output_on(cmd.freq_hz);
        } else {
            buzzer_output_off();
        }
        vTaskDelay(pdMS_TO_TICKS(cmd.duration_ms));
        buzzer_output_off();
    }
}

esp_err_t buzzer_init(gpio_num_t pin)
{
    if (s_initialized) return ESP_OK;

    ledc_timer_config_t t = {
        .speed_mode      = BUZZER_LEDC_MODE,
        .timer_num       = BUZZER_LEDC_TIMER,
        .duty_resolution = BUZZER_LEDC_RES,
        .freq_hz         = BUZZER_DEFAULT_FREQ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    esp_err_t err = ledc_timer_config(&t);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_timer_config failed: %s", esp_err_to_name(err));
        return err;
    }

    ledc_channel_config_t c = {
        .gpio_num   = pin,
        .speed_mode = BUZZER_LEDC_MODE,
        .channel    = BUZZER_LEDC_CHANNEL,
        .timer_sel  = BUZZER_LEDC_TIMER,
        .intr_type  = LEDC_INTR_DISABLE,
        .duty       = 0,
        .hpoint     = 0,
    };
    err = ledc_channel_config(&c);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_channel_config failed: %s", esp_err_to_name(err));
        return err;
    }

    s_queue = xQueueCreate(BUZZER_QUEUE_LEN, sizeof(tone_cmd_t));
    if (!s_queue) return ESP_ERR_NO_MEM;

    if (xTaskCreate(buzzer_task, "buzzer", 2048, NULL, 5, &s_task) != pdPASS) {
        vQueueDelete(s_queue);
        s_queue = NULL;
        return ESP_FAIL;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "Initialized on GPIO %d", pin);
    return ESP_OK;
}

void buzzer_tone(uint32_t freq_hz, uint32_t duration_ms)
{
    if (!s_initialized || duration_ms == 0) return;
    tone_cmd_t cmd = { .freq_hz = freq_hz, .duration_ms = duration_ms };
    xQueueSend(s_queue, &cmd, 0);
}

void buzzer_beep_pattern(const uint16_t *pattern, size_t count)
{
    if (!pattern || count == 0) return;
    for (size_t i = 0; i < count; ++i) {
        buzzer_tone(pattern[2 * i], pattern[2 * i + 1]);
    }
}

void buzzer_stop(void)
{
    if (!s_initialized) return;
    xQueueReset(s_queue);
    buzzer_output_off();
}
