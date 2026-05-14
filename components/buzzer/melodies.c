#include "melodies.h"
#include "buzzer.h"
#include <string.h>

// (freq_hz, duration_ms) pairs — freq=0 means silence.

static const uint16_t PATTERN_REMINDER[] = {
    880, 150,   0, 80,
    880, 150,   0, 80,
    880, 200,
};

// "Happy Birthday" first phrase.
static const uint16_t PATTERN_BIRTHDAY[] = {
    262, 300,   0, 40,   262, 150,   0, 40,
    294, 400,
    262, 400,
    349, 400,
    330, 700,
};

typedef struct {
    melody_id_t      id;
    const char      *name;
    const uint16_t  *pattern;
    size_t           pair_count;   // number of (freq, dur) pairs
} melody_entry_t;

#define ENTRY(id_, name_, pat_) \
    { (id_), (name_), (pat_), sizeof(pat_) / sizeof(uint16_t) / 2 }

static const melody_entry_t k_melodies[] = {
    ENTRY(MELODY_REMINDER, "reminder", PATTERN_REMINDER),
    ENTRY(MELODY_BIRTHDAY, "birthday", PATTERN_BIRTHDAY),
};

#define K_MELODIES_LEN (sizeof(k_melodies) / sizeof(k_melodies[0]))

const char *melody_name(melody_id_t id)
{
    for (size_t i = 0; i < K_MELODIES_LEN; i++) {
        if (k_melodies[i].id == id) return k_melodies[i].name;
    }
    return "reminder";
}

melody_id_t melody_from_name(const char *name)
{
    if (!name) return MELODY_REMINDER;
    for (size_t i = 0; i < K_MELODIES_LEN; i++) {
        if (strcmp(k_melodies[i].name, name) == 0) return k_melodies[i].id;
    }
    return MELODY_REMINDER;
}

bool melody_is_valid(int id)
{
    return id >= 0 && id < MELODY_COUNT;
}

void melody_play(melody_id_t id)
{
    for (size_t i = 0; i < K_MELODIES_LEN; i++) {
        if (k_melodies[i].id == id) {
            buzzer_beep_pattern(k_melodies[i].pattern, k_melodies[i].pair_count);
            return;
        }
    }
}
