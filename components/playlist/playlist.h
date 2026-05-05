#pragma once
#include "esp_err.h"
#include <stdbool.h>

#define PLAYLIST_MAX_ENTRIES 64
#define PLAYLIST_NAME_LEN    64
#define PLAYLIST_URL_LEN     256

typedef struct {
    char name[PLAYLIST_NAME_LEN];
    char url[PLAYLIST_URL_LEN];
    bool favorite;
} playlist_entry_t;

esp_err_t            playlist_load(void);
int                  playlist_get_count(void);
const playlist_entry_t* playlist_get(int index);