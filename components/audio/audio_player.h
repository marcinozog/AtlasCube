#pragma once

#include <stdbool.h>

void audio_player_init(void);

void audio_player_play(const char *url);
void audio_player_stop(void);

void audio_player_set_volume(int volume);
void audio_player_set_eq_10(int *bands);
void audio_player_set_eq_enabled(bool enabled);