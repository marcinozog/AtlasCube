#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "driver/i2c_master.h"

/**
 * Attach the CST816D to an already-created NEW I2C master bus and pulse RST.
 *
 * Pass -1 for rst_gpio to skip the hardware reset. The caller (touch.c)
 * owns the bus handle.
 */
void cst816d_init(i2c_master_bus_handle_t bus, int rst_gpio);

/**
 * Read the current touch point.
 *
 * Returns true if a finger is currently down (and writes coordinates to
 * *x and *y); false otherwise. Coordinates are in panel pixels.
 */
bool cst816d_read(uint16_t *x, uint16_t *y);
