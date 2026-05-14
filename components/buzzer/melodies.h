#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Registry of named buzzer patterns. To add a new melody:
//   1) add an entry to melody_id_t below (before MELODY_COUNT),
//   2) add a pattern + row to k_melodies[] in melodies.c.
typedef enum {
    MELODY_REMINDER = 0,         // 3× short 880 Hz tone (events: reminder/nameday)
    MELODY_BIRTHDAY,             // Happy Birthday first phrase (events: birthday/anniversary)
    MELODY_COUNT,
} melody_id_t;

// Returns canonical name like "reminder" / "dashboard_alert" — stable across builds.
const char *melody_name(melody_id_t id);

melody_id_t melody_from_name(const char *name);

bool        melody_is_valid(int id);

// Queues the melody on the buzzer (non-blocking). No-op for invalid ids.
void        melody_play(melody_id_t id);

#ifdef __cplusplus
}
#endif
