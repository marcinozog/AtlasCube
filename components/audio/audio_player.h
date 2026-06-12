#pragma once

#include <stdbool.h>

void audio_player_init(void);

void audio_player_play(const char *url);
void audio_player_stop(void);

// Plays a local audio file (WAV) from the mounted SD/FAT filesystem, e.g.
// "/sdcard/voice/v_ab12cd.wav". Unlike a radio stream the file has a real end:
// when it finishes the pipeline is torn down and the finished callback (if set)
// is invoked — the radio retry logic does NOT kick in. Used for voice
// notifications. Async, like audio_player_play.
void audio_player_play_file(const char *path);

// Registered once at init by the layer that knows how to restore the previous
// audio source (radio_service). Called from an internal task when a file
// started via audio_player_play_file reaches its end. NULL to detach.
typedef void (*audio_finished_cb_t)(void);
void audio_player_set_finished_cb(audio_finished_cb_t cb);

// Async stop: signals audio_play_task to tear the pipeline down on its own
// task/stack. Safe to call from small or latency-sensitive tasks (e.g. the BT
// UART task) — it never blocks and does no flash I/O.
void audio_player_request_stop(void);

void audio_player_set_volume(int volume);
void audio_player_set_eq_10(int *bands);
void audio_player_set_eq_enabled(bool enabled);