#pragma once
#include "audio_element.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Initializes the DSP element: 10-band EQ (peaking biquad) + volume
 *         in a single sample pass and a single FreeRTOS task.
 *
 * @param  sample_rate  Sample rate (e.g. 44100)
 * @param  channels     Number of channels (1 or 2)
 * @return audio_element_handle_t  or NULL on allocation error
 */
audio_element_handle_t audio_dsp_init(int sample_rate, int channels);

/**
 * @brief  Sets the gains for the 10 EQ bands [dB].
 *         Call from any task — no pipeline interruption.
 *
 * @param  el        DSP element handle
 * @param  gains_db  Array of 10 int values (dB), typical range −13..+13
 */
void audio_dsp_set_gains(audio_element_handle_t el, int *gains_db);

/**
 * @brief  Sets the volume level.
 *         Call from any task.
 *
 * @param  el      DSP element handle
 * @param  volume  0.0 (silence) .. 1.0 (full volume)
 */
void audio_dsp_set_volume(audio_element_handle_t el, float volume);

/**
 * @brief  Updates sample rate and channel count (call after MUSIC_INFO).
 *         Coefficient recalculation happens lazily in process().
 *
 * @param  el           DSP element handle
 * @param  sample_rate  New sample rate
 * @param  channels     New channel count
 */
void audio_dsp_set_sample_rate(audio_element_handle_t el, int sample_rate, int channels);

/**
 * @brief  Enables/disables the 10-band EQ. Volume works independently.
 *         When off: the biquad loop is fully skipped — CPU savings.
 *
 * @param  el       DSP element handle
 * @param  enabled  true = EQ active, false = bypass
 */
void audio_dsp_set_eq_enabled(audio_element_handle_t el, bool enabled);

#ifdef __cplusplus
}
#endif