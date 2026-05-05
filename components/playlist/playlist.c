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

esp_err_t playlist_load(void)
{
    s_count = 0;

    FILE *f = fopen(PLAYLIST_FILE, "r");
    if (!f) {
        ESP_LOGE(TAG, "Cannot open %s", PLAYLIST_FILE);
        return ESP_FAIL;
    }

    char line[PLAYLIST_NAME_LEN + PLAYLIST_URL_LEN + 8];

    while (fgets(line, sizeof(line), f) && s_count < PLAYLIST_MAX_ENTRIES) {
        // strip \r\n
        line[strcspn(line, "\r\n")] = 0;
        if (line[0] == 0) continue;

        // format: name\turl[\tfavorite]   (favorite: '0' or '1')
        char *tab1 = strchr(line, '\t');
        if (!tab1) continue;

        *tab1 = 0;
        char *url_start = tab1 + 1;

        char *tab2 = strchr(url_start, '\t');
        char *flag_start = NULL;
        if (tab2) {
            *tab2 = 0;
            flag_start = tab2 + 1;
        }

        strncpy(s_entries[s_count].name, line,      PLAYLIST_NAME_LEN - 1);
        strncpy(s_entries[s_count].url,  url_start, PLAYLIST_URL_LEN  - 1);

        s_entries[s_count].name[PLAYLIST_NAME_LEN - 1] = 0;
        s_entries[s_count].url [PLAYLIST_URL_LEN  - 1] = 0;
        s_entries[s_count].favorite = (flag_start && flag_start[0] == '1');

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