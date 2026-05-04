#pragma once

#include "audio_hal.h"
#include "board_def.h"
#include "board_pins_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct audio_board_handle {
    audio_hal_handle_t audio_hal;
} *audio_board_handle_t;

audio_board_handle_t audio_board_init(void);
audio_board_handle_t audio_board_get_handle(void);
esp_err_t audio_board_deinit(audio_board_handle_t audio_board);

// wymagane przez ADF
audio_hal_handle_t audio_board_codec_init(void);

#ifdef __cplusplus
}
#endif