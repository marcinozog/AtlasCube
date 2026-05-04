#include "theme.h"
#include "defines.h"
#include "cJSON.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


static const ui_theme_colors_t k_dark_defaults = {
    .bg_primary     = 0x0D0D1A,
    .bg_secondary   = 0x1A1A2E,
    .text_primary   = 0xFFFFFF,
    .text_secondary = 0xA8A8B3,
    .text_muted     = 0x505060,
    .accent         = 0xE94560,
    .bt_brand       = 0x0082FC,
    .status_ok      = 0x00C853,
};

static const ui_theme_colors_t k_light_defaults = {
    .bg_primary     = 0xF4F4F8,
    .bg_secondary   = 0xE4E4EC,
    .text_primary   = 0x0D0D1A,
    .text_secondary = 0x484860,
    .text_muted     = 0x8888A0,
    .accent         = 0xC4243E,
    .bt_brand       = 0x0066CC,
    .status_ok      = 0x1B8E3A,
};

// Mutable palettes - defaults at startup, overwritten from theme.json
static ui_theme_colors_t s_dark = {
    .bg_primary     = 0x0D0D1A,
    .bg_secondary   = 0x1A1A2E,
    .text_primary   = 0xFFFFFF,
    .text_secondary = 0xA8A8B3,
    .text_muted     = 0x505060,
    .accent         = 0xE94560,
    .bt_brand       = 0x0082FC,
    .status_ok      = 0x00C853,
};
static ui_theme_colors_t s_light = {
    .bg_primary     = 0xF4F4F8,
    .bg_secondary   = 0xE4E4EC,
    .text_primary   = 0x0D0D1A,
    .text_secondary = 0x484860,
    .text_muted     = 0x8888A0,
    .accent         = 0xC4243E,
    .bt_brand       = 0x0066CC,
    .status_ok      = 0x1B8E3A,
};

static const ui_theme_colors_t *s_current = &s_dark;
static ui_theme_t                s_theme   = THEME_DARK;

const ui_theme_colors_t *theme_get(void)    { return s_current; }
ui_theme_t               theme_current(void){ return s_theme;   }

const ui_theme_colors_t *theme_palette_get(ui_theme_t t)
{
    return (t == THEME_LIGHT) ? &s_light : &s_dark;
}

void theme_set(ui_theme_t t)
{
    s_theme   = t;
    s_current = (t == THEME_LIGHT) ? &s_light : &s_dark;
}

void theme_palette_set(ui_theme_t t, const ui_theme_colors_t *c)
{
    if (!c) return;
    if (t == THEME_LIGHT) s_light = *c;
    else                  s_dark  = *c;
}

void theme_palette_reset(ui_theme_t t)
{
    if (t == THEME_LIGHT) s_light = k_light_defaults;
    else                  s_dark  = k_dark_defaults;
}

// ── helpers ─────────────────────────────────────────────────────────────────

static uint32_t parse_hex(const char *s, uint32_t fallback)
{
    if (!s) return fallback;
    if (*s == '#') s++;
    char *end = NULL;
    unsigned long v = strtoul(s, &end, 16);
    if (!end || *end != 0) return fallback;
    return (uint32_t)(v & 0xFFFFFF);
}

static void load_palette(cJSON *obj, ui_theme_colors_t *dst, const ui_theme_colors_t *def)
{
    *dst = *def;
    if (!cJSON_IsObject(obj)) return;

    #define LOAD(field) do {                                                  \
        cJSON *it = cJSON_GetObjectItem(obj, #field);                         \
        if (cJSON_IsString(it))                                               \
            dst->field = parse_hex(it->valuestring, def->field);              \
    } while (0)

    LOAD(bg_primary);
    LOAD(bg_secondary);
    LOAD(text_primary);
    LOAD(text_secondary);
    LOAD(text_muted);
    LOAD(accent);
    LOAD(bt_brand);
    LOAD(status_ok);
    #undef LOAD
}

static void add_palette(cJSON *parent, const char *name, const ui_theme_colors_t *c)
{
    cJSON *o = cJSON_CreateObject();
    char buf[8];

    #define ADD(field) do {                                                   \
        snprintf(buf, sizeof(buf), "#%06lX",                                  \
                 (unsigned long)(c->field & 0xFFFFFF));                       \
        cJSON_AddStringToObject(o, #field, buf);                              \
    } while (0)

    ADD(bg_primary);
    ADD(bg_secondary);
    ADD(text_primary);
    ADD(text_secondary);
    ADD(text_muted);
    ADD(accent);
    ADD(bt_brand);
    ADD(status_ok);
    #undef ADD

    cJSON_AddItemToObject(parent, name, o);
}

// ── I/O ─────────────────────────────────────────────────────────────────────

esp_err_t theme_load_from_file(void)
{
    // First, defaults - missing fields in the file will not leave zeros
    s_dark  = k_dark_defaults;
    s_light = k_light_defaults;

    FILE *f = fopen(THEME_FILE, "r");
    if (!f) {
        ESP_LOGI("THEME", "theme.json missing — using default palettes");
        return ESP_FAIL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    if (size <= 0 || size > 4096) {
        fclose(f);
        ESP_LOGW("THEME", "theme.json invalid size: %ld", size);
        return ESP_FAIL;
    }

    char *buf = malloc(size + 1);
    if (!buf) {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }
    fread(buf, 1, size, f);
    buf[size] = 0;
    fclose(f);

    cJSON *json = cJSON_Parse(buf);
    free(buf);
    if (!json) {
        ESP_LOGW("THEME", "theme.json parse error — using defaults");
        return ESP_FAIL;
    }

    load_palette(cJSON_GetObjectItem(json, "dark"),  &s_dark,  &k_dark_defaults);
    load_palette(cJSON_GetObjectItem(json, "light"), &s_light, &k_light_defaults);

    cJSON_Delete(json);
    ESP_LOGI("THEME", "theme.json loaded");
    return ESP_OK;
}

esp_err_t theme_save_to_file(void)
{
    cJSON *json = cJSON_CreateObject();
    add_palette(json, "dark",  &s_dark);
    add_palette(json, "light", &s_light);

    char *str = cJSON_PrintUnformatted(json);
    if (!str) {
        cJSON_Delete(json);
        return ESP_ERR_NO_MEM;
    }

    FILE *f = fopen(THEME_FILE, "w");
    if (!f) {
        cJSON_Delete(json);
        free(str);
        return ESP_FAIL;
    }

    fwrite(str, 1, strlen(str), f);
    fclose(f);

    cJSON_Delete(json);
    free(str);
    ESP_LOGI("THEME", "theme.json saved");
    return ESP_OK;
}