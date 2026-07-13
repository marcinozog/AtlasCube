#pragma once
#include "esp_err.h"
#include <stdbool.h>

#define PLAYLIST_MAX_ENTRIES 512
#define PLAYLIST_NAME_LEN    64
#define PLAYLIST_URL_LEN     256
#define PLAYLIST_UUID_LEN    37
#define PLAYLIST_ICON_LEN    128

typedef struct {
    char name[PLAYLIST_NAME_LEN];
    char url[PLAYLIST_URL_LEN];
    bool favorite;
    char station_uuid[PLAYLIST_UUID_LEN];
    char icon_path[PLAYLIST_ICON_LEN];
} playlist_entry_t;

esp_err_t            playlist_load(void);
int                  playlist_get_count(void);
const playlist_entry_t* playlist_get(int index);
