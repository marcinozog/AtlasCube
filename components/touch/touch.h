#pragma once

/**
 * Initializes the touch component.
 *
 * Configures the I2C master bus on CTP_SCL/CTP_SDA (from defines.h),
 * resets the controller via CTP_RST, sets up the CTP_INT interrupt,
 * and registers an LVGL pointer input device.
 *
 * Call AFTER display_init() (LVGL must already be initialized).
 * If touch pins are not configured (any of CTP_SCL/CTP_SDA == -1), the
 * function logs a warning and returns without registering anything.
 */
void touch_init(void);
