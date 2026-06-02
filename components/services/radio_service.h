#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RADIO_STATE_STOPPED,
    RADIO_STATE_PLAYING,
    RADIO_STATE_BUFFERING,
    RADIO_STATE_ERROR
} radio_state_t;

void radio_service_init(void);

void radio_play_url(const char *url);
void radio_play_index(int index);
void radio_stop(void);

/**
 * Re-anchors the persisted curr_index after a playlist edit/reorder.
 * curr_index is a positional pointer; once the list is reordered (favorites,
 * drag&drop, deletions) it may address a different station. This matches the
 * currently selected/playing station by URL and moves curr_index to its new
 * position, keeping the UI highlight and prev/next in sync. No-op if nothing
 * is loaded or the station was removed.
 */
void radio_resync_curr_index(void);

/**
 * Fades the volume from 0 up to target_pct (0–100) over duration_ms.
 * Intended for the night-mode wake-up. Cancels any in-flight ramp and is
 * cancelled by radio_stop(). duration_ms <= 0 sets the volume instantly.
 */
void radio_volume_ramp_to(int target_pct, int duration_ms);

radio_state_t radio_get_state(void);
const char* radio_get_current_url(void);

#ifdef __cplusplus
}
#endif