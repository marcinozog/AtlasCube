#include "sd_cover_widget.h"
#include "lv_bin_image.h"
#include "app_state.h"
#include "cover_art.h"
#include "sdcard.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

// Two sources, one image on screen. A folder's cover.bin is read here directly
// (it is one file per album, so it only changes when the folder does); artwork
// pulled out of a track's tag is decoded by cover_art on its own task and
// arrives through UI_EVT_SD_COVER. The file wins wherever both exist.

static const char *TAG = "SD_COVER";

// Name and format are fixed on purpose — the web uploader and the converter
// both write exactly this, so nothing here has to guess between spellings.
#define SD_COVER_FILE "cover.bin"

#define COVER_DIR_LEN sizeof(((app_state_t *)0)->sd_dir)
#define COVER_KEY_LEN (COVER_DIR_LEN + sizeof(((app_state_t *)0)->sd_track) + 2)

static lv_image_dsc_t *s_dsc;
static lv_obj_t       *s_image;
static int             s_size;                 // requested on-screen size
static bool            s_from_file;            // shown artwork came from cover.bin
static char            s_dir[COVER_DIR_LEN];   // folder the file check was done for
static char            s_key[COVER_KEY_LEN];   // folder + track the state was built for

static void clear_cover(void)
{
    if (s_image) {
        lv_image_set_src(s_image, NULL);
        lv_obj_add_flag(s_image, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_dsc) lv_bin_image_free(s_dsc);
    s_dsc       = NULL;
    s_from_file = false;
}

static void show_dsc(lv_image_dsc_t *dsc, bool from_file)
{
    clear_cover();
    s_dsc       = dsc;
    s_from_file = from_file;
    lv_image_set_src(s_image, s_dsc);
    lv_obj_clear_flag(s_image, LV_OBJ_FLAG_HIDDEN);
}

// Load <dir>/cover.bin, if there is one. Leaves the widget alone and returns
// false when there is not — the tag artwork may still fill it.
static bool show_file(const char *dir)
{
    if (!dir[0]) return false;

    char path[COVER_DIR_LEN + sizeof(SD_COVER_FILE) + 1];
    snprintf(path, sizeof(path), "%s/" SD_COVER_FILE, dir);

    // A folder without a converted cover is the normal case, not an error — ask
    // first so the loader doesn't log a warning for every such album.
    struct stat st;
    if (stat(path, &st) != 0) return false;

    // Resampled at load, so the file on the card keeps one size (the uploader's
    // 240x240) whatever the layout asks for — moving the widget in the layout
    // editor never means regenerating covers.
    lv_image_dsc_t *dsc = lv_bin_image_load_scaled(path, s_size, s_size);
    if (!dsc) {
        ESP_LOGW(TAG, "Cannot load %s", path);
        return false;
    }
    show_dsc(dsc, true);
    return true;
}

// Adopt whatever cover_art has decoded out of the playing track's tag.
static void take_embedded(void)
{
    uint16_t *buf = NULL;
    int w = 0, h = 0;
    switch (cover_art_take_embedded(&buf, &w, &h)) {
        case COVER_EMB_NEW: {
            lv_image_dsc_t *dsc = lv_bin_image_wrap_rgb565(buf, w, h);
            if (dsc) show_dsc(dsc, false);
            else     heap_caps_free(buf);
            break;
        }
        case COVER_EMB_CLEAR:
            clear_cover();
            break;
        default:
            break;
    }
}

void sd_cover_widget_create(lv_obj_t *parent, int x, int y, int size)
{
    s_dir[0] = '\0';
    s_key[0] = '\0';
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
    // Only a playing track has a cover; going idle drops it so the last album's
    // artwork doesn't linger over a stopped player.
    const char *dir = state->sd_active ? state->sd_dir : "";
    if (dir[0] != '/' || strstr(dir, "..")) dir = "";

    char key[COVER_KEY_LEN];
    char track[COVER_KEY_LEN];
    track[0] = '\0';
    if (dir[0] && state->sd_track[0])
        snprintf(track, sizeof(track), "%s/%s", dir, state->sd_track);
    snprintf(key, sizeof(key), "%s", track[0] ? track : dir);
    if (strcmp(key, s_key) == 0) return;
    snprintf(s_key, sizeof(s_key), "%s", key);

    // The folder file only has to be looked at when the folder itself changed;
    // moving through an album is a track change, not a new cover.bin.
    if (strcmp(dir, s_dir) != 0) {
        snprintf(s_dir, sizeof(s_dir), "%s", dir);
        clear_cover();                     // never carry a folder's art into the next
        if (!dir[0]) return;
        if (sdcard_init() != ESP_OK) {
            ESP_LOGW(TAG, "SD unavailable for %s", dir);
            return;
        }
        if (show_file(dir)) return;        // the file outranks the tags
    }
    if (s_from_file) return;

    // No artwork file here: have the tag of this very track looked at. The
    // answer arrives as UI_EVT_SD_COVER — including "this one has none", which
    // is what clears the previous track's picture on a compilation.
    cover_art_request(dir, track[0] ? track : NULL);
}

void sd_cover_widget_reload(void)
{
    if (!s_image || !s_dir[0]) return;
    // A conversion may have just produced the folder's cover.bin; otherwise the
    // news is embedded artwork. Deliberately makes no new request — that would
    // loop with the "this track has no cover" answer.
    if (s_from_file) return;
    if (show_file(s_dir)) return;
    take_embedded();
}

void sd_cover_widget_destroy(void)
{
    clear_cover();
    s_image  = NULL;
    s_dir[0] = '\0';
    s_key[0] = '\0';
}
