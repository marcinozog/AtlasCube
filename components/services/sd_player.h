#pragma once

#include <stdbool.h>

// SD-card music player. Scans a folder for audio files (mp3/wav/flac/aac) and
// plays them as a queue over the shared audio engine. Mutually exclusive with
// the radio and BT sources — starting it takes over the I2S output.

// Scan `dir` (NULL → the default music folder) into the queue WITHOUT playing.
// Returns the track count. Used by the web UI to list files before picking one.
int sd_player_scan(const char *dir);

// Current queue accessors (valid after a scan / play_folder).
int sd_player_count(void);
const char *sd_player_track(int index);   // queue file name, or NULL if out of range

// Scan `dir` (NULL → the default music folder) and start playing from track 0.
void sd_player_play_folder(const char *dir);

// Play a specific track in the current queue (wraps out-of-range indices).
void sd_player_play_index(int index);

void sd_player_stop(void);
void sd_player_next(void);
void sd_player_prev(void);

// Auto-advance to the next track. Called by the file-finished dispatcher in
// radio_service when a track played by this module reaches its end; stops at
// the end of the queue.
void sd_player_on_track_end(void);

bool sd_player_is_active(void);
