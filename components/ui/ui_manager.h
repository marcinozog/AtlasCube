#pragma once

#include "ui_events.h"
#include "ui_screen.h"

#ifdef __cplusplus
extern "C" {
#endif

// --------------------------------------------------------------------------
// Initialization (call instead of the old ui_init from display.c)
// --------------------------------------------------------------------------

/**
 * Initializes the manager: creates the queue, registers the app_state
 * callback, registers all screens. Does NOT create any LVGL objects yet.
 * Call BEFORE starting lvgl_task.
 */
void ui_manager_init(void);

// --------------------------------------------------------------------------
// Navigation — thread-safe, callable from any task
// --------------------------------------------------------------------------

/**
 * Switch to the given screen.
 * Safe from any context — pushes an event onto the queue.
 */
void ui_navigate(ui_screen_id_t screen_id);

// --------------------------------------------------------------------------
// Sending events to UI — thread-safe
// --------------------------------------------------------------------------

/**
 * Send any event to the active screen.
 * Safe from any context (ISR: use ui_event_send_from_isr).
 */
void ui_event_send(const ui_event_t *ev);
void ui_event_send_from_isr(const ui_event_t *ev);

// --------------------------------------------------------------------------
// Physical input — call from the encoder/button task
// --------------------------------------------------------------------------

void ui_input_send(ui_input_t input);

// --------------------------------------------------------------------------
// Main loop — call from lvgl_task instead of while(1) { lv_timer_handler }
// --------------------------------------------------------------------------

/**
 * Blocking LVGL loop + event dispatcher.
 * Run as the last instruction in lvgl_task.
 */
void ui_manager_run(void);

#ifdef __cplusplus
}
#endif