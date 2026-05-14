#pragma once

#include "ui_screen.h"
#include <stdbool.h>

typedef enum {
    SCREENSAVER_CLOCKHANDS = 0,
    SCREENSAVER_STARFIELD,
    SCREENSAVER_FIREWORKS,
    SCREENSAVER_PLASMA,
    SCREENSAVER_LIFE,
    SCREENSAVER_DASHBOARD,
    SCREENSAVER_BLANK,
    SCREENSAVER_COUNT,
} screensaver_id_t;

const ui_screen_t *screensaver_get(screensaver_id_t id);
const char        *screensaver_name(screensaver_id_t id);
screensaver_id_t   screensaver_from_name(const char *name);
bool               screensaver_is_valid(int id);
