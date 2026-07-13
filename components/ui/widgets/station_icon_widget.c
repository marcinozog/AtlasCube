#include "station_icon_widget.h"
#include "now_playing_widget.h"
#include "lv_bin_image.h"
#include "playlist.h"
#include "app_state.h"
#include "sdcard.h"
#include "ui_profile.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "STATION_ICON";
static lv_image_dsc_t *s_dsc;
static char s_path[PLAYLIST_ICON_LEN];

static void clear_icon(void)
{
    now_playing_widget_set_icon(NULL);
    if (s_dsc) lv_bin_image_free(s_dsc);
    s_dsc = NULL;
}

void station_icon_widget_create(void)
{
    s_path[0] = '\0';
    s_dsc = NULL;
    station_icon_widget_update();
}

void station_icon_widget_update(void)
{
#if DISPLAY_HEIGHT <= 64
    return; // monochrome profiles do not render station artwork
#else
    const app_state_t *state = app_state_get();
    const playlist_entry_t *entry = playlist_get(state->curr_index);
    const char *rel = entry ? entry->icon_path : "";

    if (!rel[0] || rel[0] != '/' || strstr(rel, "..")) {
        if (s_path[0] || s_dsc) {
            s_path[0] = '\0';
            clear_icon();
        }
        return;
    }
    if (strcmp(rel, s_path) == 0) return;

    clear_icon();
    strncpy(s_path, rel, sizeof(s_path) - 1);
    s_path[sizeof(s_path) - 1] = '\0';

    if (sdcard_init() != ESP_OK) {
        ESP_LOGW(TAG, "SD unavailable for %s", rel);
        return;
    }
    char full[sizeof(s_path) + sizeof(SD_MOUNT_POINT) + 1];
    snprintf(full, sizeof(full), "%s%s", SD_MOUNT_POINT, rel);
    s_dsc = lv_bin_image_load(full, 64, 64);
    if (!s_dsc) {
        ESP_LOGW(TAG, "Cannot load %s", full);
        return;
    }
    now_playing_widget_set_icon(s_dsc);
#endif
}

void station_icon_widget_destroy(void)
{
    clear_icon();
    s_path[0] = '\0';
}
