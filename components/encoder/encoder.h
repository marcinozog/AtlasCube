#pragma once

/**
 * @brief Initializes the encoder component.
 *
 * Configures GPIO for CLK, DT and BTN (from defines.h),
 * installs the ISR and starts the internal task.
 *
 * Call once, before starting lvgl_task.
 * Requires a prior call to ui_manager_init().
 */
void encoder_init(void);