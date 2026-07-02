#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "driver/spi_master.h"

/**
 * Attach an XPT2046 as a device on an already-initialized SPI bus `host`,
 * driving chip-select `cs_gpio`. The bus may be dedicated (e.g. SPI3) or shared
 * with the LCD (SPI2) — in the shared case the bus must have been initialized
 * with a MISO line so the controller's ADC can be read back. Call once.
 */
void xpt2046_init(spi_host_device_t host, int cs_gpio);

/**
 * Read the current touch point as RAW 12-bit ADC values (NOT pixels — the
 * caller maps to screen coordinates via the per-profile TOUCH_RAW_* calibration).
 *
 * Returns true and writes *x / *y when a finger is pressed (measured Z pressure
 * above threshold); false otherwise.
 */
bool xpt2046_read(uint16_t *x, uint16_t *y);
