#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the on-board addressable RGB LED (WS2812B family, XL-5050RGBC
 * on ES3C28P). Wires up the RMT backend via the `led_strip` component and
 * leaves the LED off.
 *
 * Returns:
 *   - ESP_OK on success (LED ready to use)
 *   - ESP_ERR_NOT_SUPPORTED if RGB_LED_PIN < 0 (board has no RGB LED)
 *   - Any RMT/led_strip error from the underlying driver
 *
 * Idempotent — second call is a no-op.
 */
esp_err_t rgb_led_init(void);

/**
 * Set the LED to the given 8-bit RGB color and refresh (latch) immediately.
 * Brightness scaling (see rgb_led_set_brightness) is applied on top of
 * the values passed here, so {255,255,255} at brightness=50 emits ~half
 * the maximum output.
 *
 * No-op if rgb_led_init() failed or the board has no LED.
 */
void rgb_led_set(uint8_t r, uint8_t g, uint8_t b);

/**
 * Set the LED to off (0,0,0) and refresh. Convenience for `rgb_led_set(0,0,0)`.
 */
void rgb_led_off(void);

/**
 * Set a global brightness multiplier applied to every subsequent
 * rgb_led_set() call. 0 = always off, 100 = full power (default).
 * Does NOT re-emit the last color — call rgb_led_set() again to apply
 * the new brightness to a static color.
 */
void rgb_led_set_brightness(uint8_t pct);

/**
 * True when init succeeded and the LED can be driven.
 */
bool rgb_led_is_available(void);

#ifdef __cplusplus
}
#endif
