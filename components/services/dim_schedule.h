#pragma once

#include "esp_err.h"

/**
 * Time-based display dimming scheduler.
 *
 * Reads dim_schedule_t from settings (display.dim_schedule). Every 30 s checks
 * local time; between dim_time and bright_time applies dim_brightness, outside
 * the night window applies the regular display.brightness.
 *
 * Calls display_set_backlight() directly so the user's saved "day" brightness
 * is preserved.
 */
esp_err_t dim_schedule_init(void);

/** Force immediate re-evaluation (e.g. after settings change or TZ change). */
void dim_schedule_apply_now(void);
