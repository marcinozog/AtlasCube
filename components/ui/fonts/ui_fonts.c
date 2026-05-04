#include "ui_fonts.h"
#include <string.h>

typedef struct {
    const char       *id;
    const lv_font_t  *font;
} font_entry_t;

// Order matters only insofar as index = position in the UI dropdown.
// To add a new font: 1) LV_FONT_DECLARE in ui_fonts.h, 2) entry here.
static const font_entry_t k_fonts[] = {
    { "montserrat_12_pl", &lv_font_montserrat_12_pl },
    { "montserrat_14_pl", &lv_font_montserrat_14_pl },
    { "montserrat_18_pl", &lv_font_montserrat_18_pl },
    { "montserrat_48",    &lv_font_montserrat_48    },
    { "montserrat_72",    &lv_font_montserrat_72    },
    { "montserrat_80",    &lv_font_montserrat_80    },
    { "montserrat_96",    &lv_font_montserrat_96    },
};

#define K_FONTS_COUNT (sizeof(k_fonts) / sizeof(k_fonts[0]))

const lv_font_t *ui_font_by_id(const char *id)
{
    if (!id) return NULL;
    for (size_t i = 0; i < K_FONTS_COUNT; ++i) {
        if (strcmp(k_fonts[i].id, id) == 0) return k_fonts[i].font;
    }
    return NULL;
}

const char *ui_font_id(const lv_font_t *f)
{
    if (!f) return "";
    for (size_t i = 0; i < K_FONTS_COUNT; ++i) {
        if (k_fonts[i].font == f) return k_fonts[i].id;
    }
    return "";
}

int ui_font_list_count(void) { return (int)K_FONTS_COUNT; }

const char *ui_font_list_id(int i)
{
    if (i < 0 || i >= (int)K_FONTS_COUNT) return NULL;
    return k_fonts[i].id;
}
