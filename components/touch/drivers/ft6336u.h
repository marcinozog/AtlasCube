#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "driver/i2c_master.h"

/**
 * Attach the FT6336U to an already-created I2C master bus and perform
 * a hardware reset using the rst_gpio pin (must be configured as output
 * by the caller; pass -1 to skip the reset pulse).
 */
void ft6336u_init(i2c_master_bus_handle_t bus, int rst_gpio);

/**
 * Read the current touch point.
 *
 * Returns true if a finger is currently down (and writes coordinates to
 * *x and *y); false otherwise. Coordinates are in panel pixels.
 */
bool ft6336u_read(uint16_t *x, uint16_t *y);
