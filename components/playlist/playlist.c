#include "playlist.h"
#include "defines.h"
#include "esp_attr.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "PLAYLIST";

// 64 entries × (name 64B + url 256B) = 20KB — large internal BSS consumer.
// Goes to PSRAM, accessed only from tasks (load, get) — never from ISR.
EXT_RAM_BSS_ATTR static playlist_entry_t s_entries[PLAYLIST_MAX_ENTRIES];
static int s_count = 0;

// One-time migration: the playlist used to live on the www partition (/spiffs),
// where a UI re-upload/re-flash would clobber it. It now lives on /config. If the
// new location is empty but the old one still holds the user's stations (first
// boot after the OTA that moved it), copy them over so they aren't lost. Runs once;
// afterwards /config is the source of truth and the /spiffs copy is ignored.
static void playlist_seed_from_legacy(void)
{
    FILE *dst_probe = fopen(PLAYLIST_FILE, "r");
    if (dst_probe) { fclose(dst_probe); return; }   // already migrated / seeded

    FILE *src = fopen(PLAYLIST_FILE_LEGACY, "r");
    if (!src) return;                                // nothing to migrate

    FILE *dst = fopen(PLAYLIST_FILE, "w");
    if (!dst) { fclose(src); return; }

    char buf[512];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), src)) > 0) fwrite(buf, 1, n, dst);

    fclose(src);
    fclose(dst);
    ESP_LOGI(TAG, "Migrated playlist %s -> %s", PLAYLIST_FILE_LEGACY, PLAYLIST_FILE);
}

esp_err_t playlist_load(void)
{
    s_count = 0;

    playlist_seed_from_legacy();

    FILE *f = fopen(PLAYLIST_FILE, "r");
    if (!f) {
        ESP_LOGE(TAG, "Cannot open %s", PLAYLIST_FILE);
        return ESP_FAIL;
    }

    char line[PLAYLIST_NAME_LEN + PLAYLIST_URL_LEN +
              PLAYLIST_UUID_LEN + PLAYLIST_ICON_LEN + 16];

    while (fgets(line, sizeof(line), f) && s_count < PLAYLIST_MAX_ENTRIES) {
        // strip \r\n
        line[strcspn(line, "\r\n")] = 0;
        if (line[0] == 0) continue;

        // Backward-compatible TSV:
        // name\turl[\tfavorite[\tstation_uuid[\ticon_path]]]
        char *field[5] = { line, NULL, NULL, NULL, NULL };
        for (int i = 1; i < 5; i++) {
            char *tab = strchr(field[i - 1], '\t');
            if (!tab) break;
            *tab = 0;
            field[i] = tab + 1;
        }
        if (!field[1]) continue;

        memset(&s_entries[s_count], 0, sizeof(s_entries[s_count]));
        strncpy(s_entries[s_count].name, field[0], PLAYLIST_NAME_LEN - 1);
        strncpy(s_entries[s_count].url,  field[1], PLAYLIST_URL_LEN  - 1);
        if (field[3]) strncpy(s_entries[s_count].station_uuid, field[3], PLAYLIST_UUID_LEN - 1);
        if (field[4]) strncpy(s_entries[s_count].icon_path, field[4], PLAYLIST_ICON_LEN - 1);

        s_entries[s_count].name[PLAYLIST_NAME_LEN - 1] = 0;
        s_entries[s_count].url [PLAYLIST_URL_LEN  - 1] = 0;
        s_entries[s_count].station_uuid[PLAYLIST_UUID_LEN - 1] = 0;
        s_entries[s_count].icon_path[PLAYLIST_ICON_LEN - 1] = 0;
        s_entries[s_count].favorite = (field[2] && field[2][0] == '1');

        // ESP_LOGI(TAG, "[%d] %s → %s", s_count, s_entries[s_count].name, s_entries[s_count].url);
        s_count++;
    }

    fclose(f);
    ESP_LOGI(TAG, "Loaded %d stations", s_count);
    return ESP_OK;
}

int playlist_get_count(void)
{
    return s_count;
}

const playlist_entry_t* playlist_get(int index)
{
    if (index < 0 || index >= s_count) return NULL;
    return &s_entries[index];
}
