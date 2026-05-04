#pragma once

#include "driver/gpio.h"
#include "esp_err.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initializes a passive buzzer on LEDC (PWM).
 * Call once (idempotent). Subsequent calls return ESP_OK without changes.
 */
esp_err_t buzzer_init(gpio_num_t pin);

/**
 * Queues a single tone. Non-blocking — patterns are played by a dedicated task.
 * freq_hz = 0 → silence (pause of the given length).
 */
void buzzer_tone(uint32_t freq_hz, uint32_t duration_ms);

/**
 * Queues a pattern: array of (freq_hz, duration_ms) pairs.
 * count = number of PAIRS, the array must have 2*count uint16_t elements.
 *   Example triple beep:
 *     static const uint16_t BEEP_TRIPLE[] = { 880,150, 0,80, 880,150, 0,80, 880,150 };
 *     buzzer_beep_pattern(BEEP_TRIPLE, 5);
 */
void buzzer_beep_pattern(const uint16_t *pattern, size_t count);

/**
 * Clears the queue and silences the output. The currently playing tone
 * finishes (best-effort — we don't interrupt it to keep the task simple).
 */
void buzzer_stop(void);

#ifdef __cplusplus
}
#endif
