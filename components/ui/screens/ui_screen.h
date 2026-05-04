#pragma once

#include "lvgl.h"
#include "ui_events.h"

// --------------------------------------------------------------------------
// Screen interface — every screen implements this struct
// --------------------------------------------------------------------------

typedef struct {
    /**
     * Create all LVGL objects.
     * Always called from lvgl_task (thread-safe).
     * @param parent  usually lv_scr_act(), but you can pass your own container
     */
    void (*create)(lv_obj_t *parent);

    /**
     * Destroy objects and release the screen's resources.
     * Always called from lvgl_task before switching to another screen.
     */
    void (*destroy)(void);

    void (*apply_theme)(void);

    /**
     * Handle a backend event (app_state, clock, weather, ...).
     * Always called from lvgl_task after pulling from the queue.
     */
    void (*on_event)(const ui_event_t *ev);

    /**
     * Handle physical input (encoder / buttons).
     * Always called from lvgl_task.
     */
    void (*on_input)(ui_input_t input);

    /** Human-readable name for logging */
    const char *name;
} ui_screen_t;