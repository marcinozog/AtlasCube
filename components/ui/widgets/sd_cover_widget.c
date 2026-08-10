#include "sd_cover_widget.h"
#include "lv_bin_image.h"
#include "app_state.h"
#include "cover_art.h"
#include "sdcard.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

// The cover of an album is one file per folder, so the widget keys on the
// playing folder rather than on the track: moving through an album costs a
// string compare, and only a folder change re-reads the card.

static const char *TAG = "SD_COVER";

// Name and format are fixed on purpose — the web uploader writes exactly this,
// so nothing here has to guess between .jpg/.png/.bin spellings.
#define SD_COVER_FILE "cover.bin"

static lv_image_dsc_t *s_dsc;
static lv_obj_t       *s_image;
static int             s_size;                        // requested on-screen size
static char            s_dir[sizeof(((app_state_t *)0)->sd_dir)];

static void clear_cover(void)
{
    if (s_image) {
        lv_image_set_src(s_image, NULL);
        lv_obj_add_flag(s_image, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_dsc) lv_bin_image_free(s_dsc);
    s_dsc = NULL;
}

void sd_cover_widget_create(lv_obj_t *parent, int x, int y, int size)
{
    s_dir[0] = '\0';
    s_dsc    = NULL;
    s_size   = size > 0 ? size : 1;

    s_image = lv_image_create(parent);
    lv_obj_set_pos(s_image, x, y);
    lv_obj_set_size(s_image, s_size, s_size);
    lv_obj_add_flag(s_image, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_image, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    sd_cover_widget_update();
}

void sd_cover_widget_update(void)
{
    if (!s_image) return;

    const app_state_t *state = app_state_get();
    // Only a playing folder has a cover; going idle drops it so the last
    // album's artwork doesn't linger over a stopped player.
    const char *dir = state->sd_active ? state->sd_dir : "";

    if (dir[0] != '/' || strstr(dir, "..")) dir = "";
    if (strcmp(dir, s_dir) == 0) return;

    clear_cover();
    strncpy(s_dir, dir, sizeof(s_dir) - 1);
    s_dir[sizeof(s_dir) - 1] = '\0';
    if (!s_dir[0]) return;

    if (sdcard_init() != ESP_OK) {
        ESP_LOGW(TAG, "SD unavailable for %s", s_dir);
        return;
    }

    char path[sizeof(s_dir) + sizeof(SD_COVER_FILE) + 1];
    snprintf(path, sizeof(path), "%s/" SD_COVER_FILE, s_dir);

    // A folder without a converted cover is the normal case, not an error — ask
    // first so the loader doesn't log a warning for every such album. The
    // conversion of a plain cover.jpg is normally kicked off by sd_player when
    // playback starts; asking again here is what makes the artwork appear
    // without a restart when the widget is switched on mid-album. The converter
    // runs off this task and reports back with UI_EVT_SD_COVER.
    struct stat st;
    if (stat(path, &st) != 0) {
        cover_art_request(s_dir);
        return;
    }

    // Resampled at load, so the file on the card keeps one size (the uploader's
    // 240x240) whatever the layout asks for — moving the widget in the layout
    // editor never means regenerating covers.
    s_dsc = lv_bin_image_load_scaled(path, s_size, s_size);
    if (!s_dsc) {
        ESP_LOGW(TAG, "Cannot load %s", path);
        return;
    }

    lv_image_set_src(s_image, s_dsc);
    lv_obj_clear_flag(s_image, LV_OBJ_FLAG_HIDDEN);
}

void sd_cover_widget_reload(void)
{
    if (!s_image) return;
    s_dir[0] = '\0';           // force the next update to look at the card again
    sd_cover_widget_update();
}

void sd_cover_widget_destroy(void)
{
    clear_cover();
    s_image  = NULL;
    s_dir[0] = '\0';
}
