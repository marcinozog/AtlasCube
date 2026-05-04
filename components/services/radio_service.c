#include "radio_service.h"
#include "audio_player.h"
#include "app_state.h"
#include "settings.h"
#include "playlist.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "RADIO_SERVICE";



/*
void radio_service_init(void)
*/
void radio_service_init(void)
{
    app_state_update(&(app_state_patch_t){
        .has_radio = true,
        .radio_state = RADIO_STATE_STOPPED
    });
}


/*
void radio_play_url(const char *url)
*/
void radio_play_url(const char *url)
{
    if (!url) {
        ESP_LOGE(TAG, "URL is NULL");
        app_state_update(&(app_state_patch_t){
            .has_radio = true,
            .radio_state = RADIO_STATE_ERROR
        });
        return;
    }

    ESP_LOGI(TAG, "Play: %s", url);

    app_state_update(&(app_state_patch_t){
        .has_url = true,
        .url = url,
        .has_radio = true,
        .radio_state = RADIO_STATE_BUFFERING,
        .has_title = true,
        .title = ""
    });

    if(app_state_get()->bt_enable)
        settings_set_bt_enable(false);

    audio_player_play(url);

    app_state_update(&(app_state_patch_t){
        .has_radio = true,
        .radio_state = RADIO_STATE_PLAYING
    });
}

void radio_play_index(int index)
{
    const playlist_entry_t *entry = playlist_get(index);
    if (!entry) {
        ESP_LOGW("RADIO", "play_index: invalid index %d", index);
        return;
    }

    // Set station name and clear old ICY title before playback starts
    app_state_update(&(app_state_patch_t){
        .has_station_name = true,  .station_name = entry->name,
        .has_title        = true,  .title        = "",
        .has_url          = true,  .url          = entry->url,
    });

    settings_set_curr_index(index);   // save + update curr_index in state
    radio_play_url(entry->url);           // or audio_player_play(entry->url)
}


/*
void radio_stop(void)
*/
void radio_stop(void)
{
    ESP_LOGI(TAG, "Stop");

    audio_player_stop();

    app_state_update(&(app_state_patch_t){
        .has_radio = true,
        .radio_state = RADIO_STATE_STOPPED,
        .has_title = true,
        .title = ""
    });
}


/*
radio_state_t radio_get_state(void)
*/
radio_state_t radio_get_state(void)
{
    return app_state_get()->radio_state;
}


/*
const char* radio_get_current_url(void)
*/
const char* radio_get_current_url(void)
{
    return app_state_get()->url;
}