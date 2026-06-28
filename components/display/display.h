#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * Initializes LVGL, the display driver and the UI manager.
 * Does NOT start the LVGL task yet — call display_start() after registering
 * any LVGL input devices (touch, etc.) from other components.
 */
void display_init(void);

/**
 * Starts the LVGL task (pinned to CPU1). After this point, no LVGL state
 * may be touched from any other task. Call once, after display_init() and
 * after every input-device registration.
 */
void display_start(void);

void display_set_backlight(uint8_t brightness);  // 0–100

/**
 * Invert panel colours live (DCS INVON/INVOFF). The flag is XORed over each
 * driver's known-good baseline, so `false` keeps the current behaviour.
 * Safe to call from any task; colour drivers latch it and apply on the next
 * flush (same task as all their SPI commands).
 */
void display_set_invert(bool invert);

/**
 * Flip the panel 180° live (re-sends MADCTL / column-remap). Like invert, the
 * flag is latched and applied on the next flush from the LVGL task; touch
 * already follows settings.display.flip at runtime. Force a repaint afterwards
 * so the new orientation is drawn immediately.
 */
void display_set_flip(bool flip);