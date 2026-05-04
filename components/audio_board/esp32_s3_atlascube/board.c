#include "board.h"
#include "esp_log.h"
#include "audio_mem.h"

static const char *TAG = "ATLAS_CUBE_BOARD";
static audio_board_handle_t board_handle = NULL;

audio_board_handle_t audio_board_init(void)
{
    if (board_handle) return board_handle;

    board_handle = audio_calloc(1, sizeof(struct audio_board_handle));
    if (!board_handle) return NULL;

    board_handle->audio_hal = audio_board_codec_init();

    ESP_LOGI(TAG, "Custom board initialized");
    return board_handle;
}

audio_board_handle_t audio_board_get_handle(void)
{
    return board_handle;
}

esp_err_t audio_board_deinit(audio_board_handle_t audio_board)
{
    if (!audio_board) return ESP_FAIL;
    audio_free(audio_board);
    board_handle = NULL;
    return ESP_OK;
}

audio_hal_handle_t audio_board_codec_init(void)
{
    return NULL;
}