#pragma once

#include <stdbool.h>

// SD-card music player with folder browsing. Scans a folder for audio files
// (mp3/wav/flac/aac) and subfolders; plays files as a queue. Mutually exclusive
// with the radio and BT sources. Browsing (sd_player_scan) and playback are
// decoupled: playback is identified by the track's full path and the playing
// folder is re-scanned on next/prev/auto-advance, so browsing elsewhere while a
// track plays doesn't disturb the queue.

// --- Browsing (for the web UI listing) --------------------------------------
// Scan `dir` (NULL → the default music folder) into the listing buffers WITHOUT
// playing. Returns the file count; folders are available via the accessors.
int sd_player_scan(const char *dir);

int  sd_player_count(void);                 // audio files in the last scan
const char *sd_player_track(int index);     // file name, or NULL
int  sd_player_folder_count(void);          // subfolders in the last scan
const char *sd_player_folder(int index);    // subfolder name, or NULL
const char *sd_player_dir(void);            // last scanned dir
const char *sd_player_root(void);           // browse root (default music folder)

// --- Playback ---------------------------------------------------------------
// Play a specific file (absolute path under the SD mount). Its folder becomes
// the playback queue.
void sd_player_play_path(const char *path);

// Scan `dir` (NULL → default) and play its first track.
void sd_player_play_folder(const char *dir);

void sd_player_stop(void);
void sd_player_next(void);
void sd_player_prev(void);

// Replay the current track (same folder + index). Used by the voice-notification
// restore path to resume SD music after the notification finishes. No-op if no
// track was queued.
void sd_player_resume_current(void);

// Pause/resume the current track (toggle). Shuffle (toggle) and repeat (cycle
// none → all → one) affect next/auto-advance selection.
void sd_player_toggle_pause(void);
void sd_player_toggle_shuffle(void);
void sd_player_cycle_repeat(void);

// Auto-advance to the next track in the playing folder (called by the
// file-finished dispatcher); stops at the end of the folder.
void sd_player_on_track_end(void);

bool sd_player_is_active(void);
