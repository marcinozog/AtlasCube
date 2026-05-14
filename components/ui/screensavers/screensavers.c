#include "screensavers.h"
#include <string.h>

extern const ui_screen_t screensaver_clockhands;
extern const ui_screen_t screensaver_starfield;
extern const ui_screen_t screensaver_fireworks;
extern const ui_screen_t screensaver_plasma;
extern const ui_screen_t screensaver_life;
extern const ui_screen_t screensaver_dashboard;
extern const ui_screen_t screensaver_blank;

static const struct {
    screensaver_id_t        id;
    const ui_screen_t      *screen;
    const char             *name;
} k_map[] = {
    { SCREENSAVER_CLOCKHANDS, &screensaver_clockhands, "clockhands" },
    { SCREENSAVER_STARFIELD,  &screensaver_starfield,  "starfield"  },
    { SCREENSAVER_FIREWORKS,  &screensaver_fireworks,  "fireworks"  },
    { SCREENSAVER_PLASMA,     &screensaver_plasma,     "plasma"     },
    { SCREENSAVER_LIFE,       &screensaver_life,       "life"       },
    { SCREENSAVER_DASHBOARD,  &screensaver_dashboard,  "dashboard"  },
    { SCREENSAVER_BLANK,      &screensaver_blank,      "blank"      },
};

#define K_MAP_LEN (sizeof(k_map) / sizeof(k_map[0]))

const ui_screen_t *screensaver_get(screensaver_id_t id)
{
    for (size_t i = 0; i < K_MAP_LEN; i++) {
        if (k_map[i].id == id) return k_map[i].screen;
    }
    return &screensaver_clockhands;
}

const char *screensaver_name(screensaver_id_t id)
{
    for (size_t i = 0; i < K_MAP_LEN; i++) {
        if (k_map[i].id == id) return k_map[i].name;
    }
    return "clockhands";
}

screensaver_id_t screensaver_from_name(const char *name)
{
    if (!name) return SCREENSAVER_CLOCKHANDS;
    for (size_t i = 0; i < K_MAP_LEN; i++) {
        if (strcmp(k_map[i].name, name) == 0) return k_map[i].id;
    }
    return SCREENSAVER_CLOCKHANDS;
}

bool screensaver_is_valid(int id)
{
    return id >= 0 && id < SCREENSAVER_COUNT;
}
