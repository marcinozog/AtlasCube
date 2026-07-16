#pragma once

#include "ui_screen.h"

typedef enum {
    LAYOUT_EDITOR_RADIO = 0,
} layout_editor_target_t;

extern const ui_screen_t screen_layout_editor;

// Select the profile section to edit, then navigate to the shared editor.
// More targets (Home / SD) can reuse the same screen and descriptor model.
void screen_layout_editor_open(layout_editor_target_t target);
