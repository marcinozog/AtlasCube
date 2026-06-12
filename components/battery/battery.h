#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the battery voltage monitor on BAT_ADC_PIN (from defines.h).
 *
 * Uses the ADC oneshot driver (esp_adc/adc_oneshot.h) on ADC1 with 12 dB
 * attenuation, plus the curve-fitting calibration scheme available on
 * ESP32-S3. The hardware divider on ES3C28P is 1:1 (R1 = R2), so the raw
 * reading is half of Vbat — the multiplication is applied internally;
 * callers always get the *actual* battery voltage in mV.
 *
 * Returns:
 *   - ESP_OK on success
 *   - ESP_ERR_NOT_SUPPORTED when BAT_ADC_PIN < 0 (board has no battery)
 *   - Any ADC unit / channel / calibration error from the IDF driver
 *
 * Idempotent — calling again after success is a no-op.
 */
esp_err_t battery_init(void);

/**
 * Read the current battery voltage in millivolts.
 *
 * Averages a small burst of ADC samples (~16) to suppress noise. The result
 * already includes the ×2 voltage-divider compensation.
 *
 * Returns -1 if the monitor is not initialized or the ADC read fails.
 */
int battery_read_mv(void);

/**
 * Read the current battery state-of-charge in percent (0..100).
 *
 * Maps battery_read_mv() through a piecewise-linear Li-Po SoC curve
 * (4.20 V → 100 %, 3.30 V → 0 %). Returns -1 on error.
 *
 * NOTE: this is a coarse estimate based on resting voltage. The real SoC
 * depends on load, temperature and cell wear — anything below ~5 % should
 * be treated as "shut down soon" regardless of the exact number.
 */
int battery_read_percent(void);

/**
 * True when battery_init() succeeded.
 */
bool battery_is_available(void);

#ifdef __cplusplus
}
#endif
