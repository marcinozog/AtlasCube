#pragma once

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