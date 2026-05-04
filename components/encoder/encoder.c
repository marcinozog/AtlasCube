#include "encoder.h"
#include "defines.h"
#include "ui_manager.h"
#include "ui_events.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "ENCODER";

// --------------------------------------------------------------------------
// Tuneable
// --------------------------------------------------------------------------

#define ENC_QUEUE_SIZE      32
#define ENC_TASK_STACK      2048
#define ENC_TASK_PRIORITY   6       // higher than ui_manager (5), lower than audio

#define DEBOUNCE_BTN_MS     70      // 50 ms between consecutive button events

#define ENC_PULSES_PER_STEP  4   // increase to slow down, decrease to speed up

#define LONG_PRESS_MS  600

// --------------------------------------------------------------------------
// Internal event type
// --------------------------------------------------------------------------

typedef enum {
    ENC_EVT_CW,         // clockwise rotation
    ENC_EVT_CCW,        // counter-clockwise rotation
    ENC_EVT_BTN,        // button press
    ENC_EVT_BTN_DOWN,   // physical press
    ENC_EVT_BTN_UP,     // physical release
} enc_raw_evt_t;

static QueueHandle_t s_queue = NULL;

// --------------------------------------------------------------------------
// Quadrature lookup table: [prev_state << 2 | new_state]
//  +1 = CW, -1 = CCW, 0 = noise / invalid transition
// --------------------------------------------------------------------------

static const int8_t ENC_TABLE[16] = {
     0, -1, +1,  0,
    +1,  0,  0, -1,
    -1,  0,  0, +1,
     0, +1, -1,  0,
};

// --------------------------------------------------------------------------
// Encoder ISR — triggered on both edges of CLK and DT
//
// 2-bit state: bit1 = CLK, bit0 = DT
// The state machine rejects invalid transitions (mechanical noise).
// --------------------------------------------------------------------------

static void IRAM_ATTR isr_rot(void *arg)
{
    static uint8_t state = 0;
    static int8_t  accum = 0;   // step accumulator

    uint8_t s = (gpio_get_level(ENC_CLK) << 1) | gpio_get_level(ENC_DT);
    uint8_t idx = (state << 2) | s;
    state = s;

    int8_t dir = ENC_TABLE[idx];
    if (dir == 0) return;

    accum += dir;
    if (accum < ENC_PULSES_PER_STEP && accum > -ENC_PULSES_PER_STEP) return;
    accum = 0;

    enc_raw_evt_t evt = (dir > 0) ? ENC_EVT_CW : ENC_EVT_CCW;
    BaseType_t woken = pdFALSE;
    xQueueSendFromISR(s_queue, &evt, &woken);
    portYIELD_FROM_ISR(woken);
}

// --------------------------------------------------------------------------
// Button ISR — falling edge (press)
// Debounce is handled by the task to keep the ISR non-blocking.
// --------------------------------------------------------------------------

static void IRAM_ATTR isr_btn(void *arg)
{
    enc_raw_evt_t evt = gpio_get_level(ENC_BTN) ? ENC_EVT_BTN_UP : ENC_EVT_BTN_DOWN;
    BaseType_t woken = pdFALSE;
    xQueueSendFromISR(s_queue, &evt, &woken);
    portYIELD_FROM_ISR(woken);
}

// --------------------------------------------------------------------------
// Task — receives events from the queue and sends them to ui_manager
// --------------------------------------------------------------------------

static void encoder_task(void *arg)
{
    enc_raw_evt_t evt;
    int64_t btn_down_ms  = 0;
    int64_t last_down_ms = 0;
    bool    long_fired   = false;

    while (1) {
        /* Compute timeout for the next wait */
        TickType_t wait = portMAX_DELAY;
        if (btn_down_ms != 0 && !long_fired) {
            int64_t elapsed_ms = esp_timer_get_time() / 1000 - btn_down_ms;
            int64_t remain_ms  = LONG_PRESS_MS - elapsed_ms;
            wait = (remain_ms > 0) ? pdMS_TO_TICKS(remain_ms) : 0;
        }

        if (xQueueReceive(s_queue, &evt, wait) != pdTRUE) {
            /* Timeout — button still held, long-press threshold reached */
            if (btn_down_ms != 0 && !long_fired) {
                long_fired = true;
                ui_input_send(UI_INPUT_ENCODER_LONG_PRESS);
            }
            continue;
        }

        if (evt == ENC_EVT_BTN_DOWN) {
            int64_t now_ms = esp_timer_get_time() / 1000;
            if (now_ms - last_down_ms < DEBOUNCE_BTN_MS) continue;
            last_down_ms = btn_down_ms = now_ms;
            long_fired   = false;
            continue;
        }

        if (evt == ENC_EVT_BTN_UP) {
            last_down_ms = esp_timer_get_time() / 1000;
            if (btn_down_ms == 0) continue;          /* UP without DOWN — ignore */
            if (!long_fired)
                ui_input_send(UI_INPUT_ENCODER_PRESS); /* short press */
            btn_down_ms = 0;
            long_fired  = false;
            continue;
        }

        /* Rotation — unchanged */
        int delta = (evt == ENC_EVT_CW) ? +1 : -1;
        enc_raw_evt_t next;
        while (xQueueReceive(s_queue, &next, 0) == pdTRUE) {
            if (next == ENC_EVT_BTN_DOWN || next == ENC_EVT_BTN_UP) {
                xQueueSendToFront(s_queue, &next, 0);
                break;
            }
            delta += (next == ENC_EVT_CW) ? +1 : -1;
        }

        int steps = (delta > 0) ? delta : -delta;
        if (steps > 4) steps = 4;
        ui_input_t dir = (delta > 0) ? UI_INPUT_ENCODER_CW : UI_INPUT_ENCODER_CCW;
        for (int i = 0; i < steps; i++) ui_input_send(dir);
    }

    vTaskDelete(NULL);
}


// --------------------------------------------------------------------------
// Public API
// --------------------------------------------------------------------------

void encoder_init(void)
{
    s_queue = xQueueCreate(ENC_QUEUE_SIZE, sizeof(enc_raw_evt_t));
    configASSERT(s_queue);

    // ── CLK (ISR trigger on both edges) ─────────────────────────────────
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << ENC_CLK),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_ANYEDGE,
    };
    gpio_config(&io);

    // ── DT (ISR trigger on both edges) ──────────────────────────────────
    io.pin_bit_mask = (1ULL << ENC_DT);
    io.intr_type    = GPIO_INTR_ANYEDGE;
    gpio_config(&io);

    // ── BTN (ISR trigger on falling edge) ────────────────────────────────
    io.pin_bit_mask = (1ULL << ENC_BTN);
    io.intr_type    = GPIO_INTR_ANYEDGE;
    gpio_config(&io);

    // ── ISR service ──────────────────────────────────────────────────────
    // gpio_install_isr_service can be called only once — ignore the error
    // if another component (e.g. display) has already done it.
    esp_err_t ret = gpio_install_isr_service(0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "gpio_install_isr_service failed: %s", esp_err_to_name(ret));
    }

    gpio_isr_handler_add(ENC_CLK, isr_rot, NULL);
    gpio_isr_handler_add(ENC_DT,  isr_rot, NULL);
    gpio_isr_handler_add(ENC_BTN, isr_btn, NULL);

    xTaskCreate(encoder_task, "encoder_task", ENC_TASK_STACK, NULL, ENC_TASK_PRIORITY, NULL);

    ESP_LOGI(TAG, "Initialized — CLK=%d  DT=%d  BTN=%d", ENC_CLK, ENC_DT, ENC_BTN);
}
