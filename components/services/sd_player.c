#include "sd_player.h"
#include "sdcard.h"
#include "audio_file_player.h"
#include "audio_engine.h"
#include "app_state.h"
#include "settings.h"
#include "radio_service.h"
#include "bt.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <dirent.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>

static const char *TAG = "SD_PLAYER";

#define SD_MUSIC_DIR   SD_MOUNT_POINT "/music"
#define SD_MAX_TRACKS  256
#define SD_NAME_MAX    128

// Queue lives in PSRAM — 256 * 128 = 32 KB is far too much for the constrained
// internal RAM, and sequential access here is fine.
static char (*s_queue)[SD_NAME_MAX] = NULL;
static int  s_count = 0;
static int  s_index = -1;
static char s_dir[128];


static bool has_audio_ext(const char *name)
{
    return strcasestr(name, ".mp3")  || strcasestr(name, ".wav")  ||
           strcasestr(name, ".flac") || strcasestr(name, ".aac")  ||
           strcasestr(name, ".aacp");
}


static int cmp_name(const void *a, const void *b)
{
    return strcasecmp((const char *)a, (const char *)b);
}


/*
static int scan_dir(const char *dir)
Reads the folder into the (PSRAM) queue, audio files only, sorted by name.
Returns the track count. Allocates the queue lazily on first use.
*/
static int scan_dir(const char *dir)
{
    if (!s_queue) {
        s_queue = heap_caps_malloc((size_t)SD_MAX_TRACKS * SD_NAME_MAX, MALLOC_CAP_SPIRAM);
        if (!s_queue) {
            ESP_LOGE(TAG, "Queue alloc failed");
            return 0;
        }
    }

    DIR *d = opendir(dir);
    if (!d) {
        ESP_LOGW(TAG, "opendir(%s) failed", dir);
        return 0;
    }

    int n = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL && n < SD_MAX_TRACKS) {
        if (e->d_type == DT_DIR)      continue;
        if (!has_audio_ext(e->d_name)) continue;
        strncpy(s_queue[n], e->d_name, SD_NAME_MAX - 1);
        s_queue[n][SD_NAME_MAX - 1] = 0;
        n++;
    }
    closedir(d);

    qsort(s_queue, n, SD_NAME_MAX, cmp_name);
    ESP_LOGI(TAG, "Scanned %s → %d tracks", dir, n);
    return n;
}


/*
static void take_over_output(void)
Claim the I2S output from whatever source was active. The engine relink (to the
file source) tears down the radio stream itself; here we just un-mux BT and
reflect the source change in app_state.
*/
static void take_over_output(void)
{
    app_state_t *s = app_state_get();

    if (s->bt_enable) {
        if (s->bt_auto_switch) bt_pause();
        settings_set_bt_enable(false);
    }

    if (s->radio_state == RADIO_STATE_PLAYING ||
        s->radio_state == RADIO_STATE_BUFFERING) {
        app_state_update(&(app_state_patch_t){
            .has_radio = true, .radio_state = RADIO_STATE_STOPPED
        });
    }
}


void sd_player_play_index(int index)
{
    if (s_count <= 0) {
        ESP_LOGW(TAG, "play_index: empty queue");
        return;
    }

    // Wrap so next/prev at the ends roll over.
    if (index < 0)        index = s_count - 1;
    if (index >= s_count) index = 0;
    s_index = index;

    take_over_output();

    char path[256];
    snprintf(path, sizeof(path), "%s/%s", s_dir, s_queue[index]);

    app_state_update(&(app_state_patch_t){
        .has_sd_active = true, .sd_active = true,
        .has_sd_index  = true, .sd_index  = index,
        .has_sd_count  = true, .sd_count  = s_count,
        .has_sd_track  = true, .sd_track  = s_queue[index],
        .has_title     = true, .title     = s_queue[index],
    });

    ESP_LOGI(TAG, "Play [%d/%d]: %s", index + 1, s_count, s_queue[index]);
    audio_file_player_play(path);
}


int sd_player_scan(const char *dir)
{
    if (!sdcard_is_mounted()) {
        ESP_LOGW(TAG, "No SD card mounted");
        s_count = 0;
        return 0;
    }

    strncpy(s_dir, dir ? dir : SD_MUSIC_DIR, sizeof(s_dir) - 1);
    s_dir[sizeof(s_dir) - 1] = 0;

    s_count = scan_dir(s_dir);
    return s_count;
}


int sd_player_count(void)
{
    return s_count;
}


const char *sd_player_track(int index)
{
    return (index >= 0 && index < s_count) ? s_queue[index] : NULL;
}


void sd_player_play_folder(const char *dir)
{
    if (sd_player_scan(dir) == 0) {
        ESP_LOGW(TAG, "No audio files in %s", s_dir);
        return;
    }
    sd_player_play_index(0);
}


void sd_player_stop(void)
{
    if (!app_state_get()->sd_active) return;

    audio_engine_request_stop();
    s_index = -1;

    app_state_update(&(app_state_patch_t){
        .has_sd_active = true, .sd_active = false,
        .has_title     = true, .title     = "",
    });
    ESP_LOGI(TAG, "Stopped");
}


void sd_player_next(void)
{
    if (s_count <= 0) return;
    sd_player_play_index((s_index + 1) % s_count);
}


void sd_player_prev(void)
{
    if (s_count <= 0) return;
    sd_player_play_index((s_index - 1 + s_count) % s_count);
}


void sd_player_on_track_end(void)
{
    if (s_count <= 0) return;

    int next = s_index + 1;
    if (next >= s_count) {
        ESP_LOGI(TAG, "Queue finished");
        s_index = -1;
        app_state_update(&(app_state_patch_t){
            .has_sd_active = true, .sd_active = false,
            .has_title     = true, .title     = "",
        });
        return;
    }

    sd_player_play_index(next);
}


bool sd_player_is_active(void)
{
    return app_state_get()->sd_active;
}
