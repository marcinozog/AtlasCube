#pragma once

#include <stdbool.h>

void audio_player_init(void);

void audio_player_play(const char *url);
void audio_player_stop(void);

// Async stop: signals audio_play_task to tear the pipeline down on its own
// task/stack. Safe to call from small or latency-sensitive tasks (e.g. the BT
// UART task) — it never blocks and does no flash I/O.
void audio_player_request_stop(void);

void audio_player_set_volume(int volume);
void audio_player_set_eq_10(int *bands);
void audio_player_set_eq_enabled(bool enabled);