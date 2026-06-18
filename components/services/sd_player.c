#include "sd_player.h"
#include "sdcard.h"
#include "audio_file_player.h"
#include "audio_engine.h"
#include "id3.h"
#include "app_state.h"
#include "settings.h"
#include "radio_service.h"
#include "bt.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_random.h"
#include <dirent.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>

static const char *TAG = "SD_PLAYER";

#define SD_MUSIC_DIR   SD_MOUNT_POINT "/music"
#define SD_MAX_TRACKS  256
#define SD_MAX_FOLDERS 128
#define SD_NAME_MAX    128
#define SD_DIR_MAX     192

// Scan buffers live in PSRAM — far too much for the constrained internal RAM,
// and sequential access here is fine. Allocated lazily on first use. These hold
// the result of the LAST scan (browse listing or a playback re-scan); the web UI
// keeps its own copy, so overwriting them on a playback re-scan is harmless.
static char (*s_queue)[SD_NAME_MAX]   = NULL;
static char (*s_folders)[SD_NAME_MAX] = NULL;
static int  s_count        = 0;   // audio files in the last scan
static int  s_folder_count = 0;   // subfolders in the last scan
static char s_dir[SD_DIR_MAX];    // last browsed dir (reported to the UI)

// Playback context (the folder of the currently playing track). Independent of
// the browse dir above.
static char s_play_dir[SD_DIR_MAX];
static int  s_play_index = -1;
static int  s_play_count = 0;

// Modes.
typedef enum { SD_REPEAT_NONE = 0, SD_REPEAT_ALL, SD_REPEAT_ONE } sd_repeat_t;
static bool        s_shuffle = false;
static sd_repeat_t s_repeat  = SD_REPEAT_NONE;


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
Reads `dir` into s_queue (audio files) and s_folders (subdirectories), both
sorted by name. Sets s_folder_count, returns the file count. Lazily allocates
the PSRAM buffers.
*/
static int scan_dir(const char *dir)
{
    if (!s_queue) {
        s_queue = heap_caps_malloc((size_t)SD_MAX_TRACKS * SD_NAME_MAX, MALLOC_CAP_SPIRAM);
    }
    if (!s_folders) {
        s_folders = heap_caps_malloc((size_t)SD_MAX_FOLDERS * SD_NAME_MAX, MALLOC_CAP_SPIRAM);
    }
    if (!s_queue || !s_folders) {
        ESP_LOGE(TAG, "Scan buffer alloc failed");
        s_count = s_folder_count = 0;
        return 0;
    }

    DIR *d = opendir(dir);
    if (!d) {
        ESP_LOGW(TAG, "opendir(%s) failed", dir);
        s_folder_count = 0;
        return 0;
    }

    int nf = 0, nd = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_type == DT_DIR) {
            if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
            if (nd < SD_MAX_FOLDERS) {
                strncpy(s_folders[nd], e->d_name, SD_NAME_MAX - 1);
                s_folders[nd][SD_NAME_MAX - 1] = 0;
                nd++;
            }
        } else if (has_audio_ext(e->d_name)) {
            if (nf < SD_MAX_TRACKS) {
                strncpy(s_queue[nf], e->d_name, SD_NAME_MAX - 1);
                s_queue[nf][SD_NAME_MAX - 1] = 0;
                nf++;
            }
        }
    }
    closedir(d);

    qsort(s_queue,   nf, SD_NAME_MAX, cmp_name);
    qsort(s_folders, nd, SD_NAME_MAX, cmp_name);

    s_folder_count = nd;
    ESP_LOGI(TAG, "Scanned %s → %d tracks, %d folders", dir, nf, nd);
    return nf;
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
        // Pause the phone unconditionally: playing an SD track is an explicit
        // takeover, so the BT source should actually stop (not keep playing
        // muted behind the hardware mux), regardless of bt_auto_switch.
        bt_pause();
        // Quiet: mux + state only, NO app_state broadcast here. This call runs
        // deep on the httpd task stack; a full-state JSON build this deep
        // overflows it. play_at() carries the bt_enable=false change in its own
        // single, shallower app_state_update instead.
        settings_set_bt_enable_quiet(false);
    }

    if (s->radio_state == RADIO_STATE_PLAYING ||
        s->radio_state == RADIO_STATE_BUFFERING) {
        app_state_update(&(app_state_patch_t){
            .has_radio = true, .radio_state = RADIO_STATE_STOPPED
        });
    }
}


// Random index in [0,n), avoiding `cur` when possible.
static int random_index(int n, int cur)
{
    if (n <= 1) return 0;
    int r = cur;
    while (r == cur) r = (int)(esp_random() % (uint32_t)n);
    return r;
}


/*
static void play_at(int idx, int n)
Start the track at s_queue[idx] within s_play_dir. The caller must have just
scanned s_play_dir into s_queue (so idx/n are consistent).
*/
static void play_at(int idx, int n)
{
    s_play_index = idx;
    s_play_count = n;

    take_over_output();

    char path[SD_DIR_MAX + SD_NAME_MAX];
    snprintf(path, sizeof(path), "%s/%s", s_play_dir, s_queue[idx]);

    // Display title from the ID3 tag ("Artist - Title"); fall back to the file
    // name when there's no usable tag. sd_track stays the file name (the queue
    // / browser key used by the web + Android UIs).
    char title[128];
    if (!id3_read_title(path, title, sizeof(title)))
        snprintf(title, sizeof(title), "%s", s_queue[idx]);

    app_state_update(&(app_state_patch_t){
        .has_sd_active = true, .sd_active = true,
        .has_sd_index  = true, .sd_index  = idx,
        .has_sd_count  = true, .sd_count  = n,
        .has_sd_track  = true, .sd_track  = s_queue[idx],
        .has_sd_dir    = true, .sd_dir    = s_play_dir,
        .has_sd_paused = true, .sd_paused = false,
        // SD is the source now → reflect BT off here (take_over_output muxed it
        // off quietly), in this one shallow broadcast.
        .has_bt_enable = true, .bt_enable = false,
        .has_title     = true, .title     = title,
    });

    ESP_LOGI(TAG, "Play [%d/%d]: %s/%s", idx + 1, n, s_play_dir, s_queue[idx]);
    audio_file_player_play(path);
}


// Mark the SD source inactive. forget_queue: also drop the playing folder/index
// (true when leaving SD for good); keep it false for the on-screen Stop so
// sd_player_resume_current() can replay the current track.
static void clear_play_state(bool forget_queue)
{
    if (forget_queue) {
        s_play_dir[0]  = 0;
        s_play_index   = -1;
        s_play_count   = 0;
    }
    app_state_update(&(app_state_patch_t){
        .has_sd_active = true, .sd_active = false,
        .has_sd_paused = true, .sd_paused = false,
        .has_title     = true, .title     = "",
    });
}


// ── Browsing ────────────────────────────────────────────────────────────────

int sd_player_scan(const char *dir)
{
    if (sdcard_init() != ESP_OK) {   // lazy mount on first SD use
        ESP_LOGW(TAG, "No SD card available");
        s_count = s_folder_count = 0;
        return 0;
    }
    strncpy(s_dir, dir ? dir : SD_MUSIC_DIR, sizeof(s_dir) - 1);
    s_dir[sizeof(s_dir) - 1] = 0;

    s_count = scan_dir(s_dir);
    return s_count;
}

int  sd_player_count(void)              { return s_count; }
const char *sd_player_track(int i)      { return (i >= 0 && i < s_count) ? s_queue[i] : NULL; }
int  sd_player_folder_count(void)       { return s_folder_count; }
const char *sd_player_folder(int i)     { return (i >= 0 && i < s_folder_count) ? s_folders[i] : NULL; }
const char *sd_player_dir(void)         { return s_dir; }
const char *sd_player_root(void)        { return SD_MUSIC_DIR; }


// ── Playback ─────────────────────────────────────────────────────────────────

void sd_player_play_path(const char *path)
{
    if (!path) return;

    const char *slash = strrchr(path, '/');
    if (!slash || slash == path) {
        ESP_LOGW(TAG, "play_path: bad path %s", path);
        return;
    }

    size_t dl = slash - path;
    if (dl >= sizeof(s_play_dir)) return;
    memcpy(s_play_dir, path, dl);
    s_play_dir[dl] = 0;
    const char *name = slash + 1;

    int n = scan_dir(s_play_dir);
    if (n <= 0) {
        ESP_LOGW(TAG, "play_path: no tracks in %s", s_play_dir);
        return;
    }

    int idx = 0;
    for (int i = 0; i < n; i++) {
        if (!strcmp(s_queue[i], name)) { idx = i; break; }
    }
    play_at(idx, n);
}


void sd_player_play_folder(const char *dir)
{
    strncpy(s_play_dir, dir ? dir : SD_MUSIC_DIR, sizeof(s_play_dir) - 1);
    s_play_dir[sizeof(s_play_dir) - 1] = 0;

    int n = scan_dir(s_play_dir);
    if (n <= 0) {
        ESP_LOGW(TAG, "No audio files in %s", s_play_dir);
        return;
    }
    play_at(0, n);
}


void sd_player_stop(void)
{
    if (!sd_player_is_active()) return;
    audio_engine_request_stop();
    clear_play_state(true);
    ESP_LOGI(TAG, "Stopped");
}


// On-screen Stop (Stop/Play toggle): tear down audio but keep the playing
// folder/index so sd_player_resume_current() can replay the current track.
void sd_player_stop_keep(void)
{
    if (!sd_player_is_active()) return;
    audio_engine_request_stop();
    clear_play_state(false);
    ESP_LOGI(TAG, "Stopped (queue kept)");
}


// Replay the current track (same folder + index). Used to bring SD music back
// after a voice notification borrowed the output. No-op if nothing was queued.
void sd_player_resume_current(void)
{
    if (!s_play_dir[0] || s_play_index < 0) return;
    int n = scan_dir(s_play_dir);
    if (n <= 0) return;
    play_at(s_play_index < n ? s_play_index : 0, n);
}


void sd_player_next(void)
{
    if (!s_play_dir[0]) return;
    int n = scan_dir(s_play_dir);          // re-derive the playing folder
    if (n <= 0) return;
    int idx = s_shuffle ? random_index(n, s_play_index) : (s_play_index + 1) % n;
    play_at(idx, n);
}


void sd_player_prev(void)
{
    if (!s_play_dir[0]) return;
    int n = scan_dir(s_play_dir);
    if (n <= 0) return;
    play_at((s_play_index - 1 + n) % n, n);
}


void sd_player_on_track_end(void)
{
    if (!s_play_dir[0]) return;
    int n = scan_dir(s_play_dir);
    if (n <= 0) return;

    if (s_repeat == SD_REPEAT_ONE) {
        play_at(s_play_index >= 0 && s_play_index < n ? s_play_index : 0, n);
        return;
    }

    int idx;
    if (s_shuffle) {
        idx = random_index(n, s_play_index);   // continuous random
    } else {
        idx = s_play_index + 1;
        if (idx >= n) {
            if (s_repeat == SD_REPEAT_ALL) {
                idx = 0;
            } else {
                ESP_LOGI(TAG, "Folder finished");
                clear_play_state(true);
                return;
            }
        }
    }
    play_at(idx, n);
}


static void publish_modes(void)
{
    app_state_update(&(app_state_patch_t){
        .has_sd_shuffle = true, .sd_shuffle = s_shuffle,
        .has_sd_repeat  = true, .sd_repeat  = (int)s_repeat,
    });
}


void sd_player_toggle_pause(void)
{
    if (!sd_player_is_active()) return;
    if (audio_engine_is_paused()) {
        audio_engine_resume();
        app_state_update(&(app_state_patch_t){ .has_sd_paused = true, .sd_paused = false });
    } else {
        audio_engine_pause();
        app_state_update(&(app_state_patch_t){ .has_sd_paused = true, .sd_paused = true });
    }
}


void sd_player_toggle_shuffle(void)
{
    s_shuffle = !s_shuffle;
    ESP_LOGI(TAG, "Shuffle %s", s_shuffle ? "on" : "off");
    publish_modes();
}


void sd_player_cycle_repeat(void)
{
    s_repeat = (sd_repeat_t)(((int)s_repeat + 1) % 3);
    ESP_LOGI(TAG, "Repeat %d", (int)s_repeat);
    publish_modes();
}


bool sd_player_is_active(void)
{
    return app_state_get()->sd_active;
}

bool sd_player_has_queue(void)
{
    return s_play_dir[0] && s_play_index >= 0;
}

void sd_player_forget(void)
{
    if (sd_player_is_active()) return;   // active → caller should sd_player_stop()
    s_play_dir[0] = 0;
    s_play_index  = -1;
    s_play_count  = 0;
}
