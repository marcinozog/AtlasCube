#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MEDIA_SOURCE_RADIO = 0,
    MEDIA_SOURCE_BT,
    MEDIA_SOURCE_SD,
} media_source_t;

typedef enum {
    MEDIA_ACTION_PLAY = 0,      // explicit play: starts/resumes, no-op when already playing
    MEDIA_ACTION_PLAY_TOGGLE,   // play/stop toggle (stop tears down, keeps the queue)
    MEDIA_ACTION_PLAY_PAUSE,    // in-place pause/resume where the source supports it
    MEDIA_ACTION_STOP,
    MEDIA_ACTION_NEXT,
    MEDIA_ACTION_PREVIOUS,
    MEDIA_ACTION_VOLUME_UP,
    MEDIA_ACTION_VOLUME_DOWN,
} media_action_t;

/**
 * Which source a semantic transport command should act on right now:
 * SD when the SD player is active, BT when enabled, radio otherwise.
 * Lets clients (voice, MQTT, WS remote) send plain "next"/"play" without
 * knowing what the device is playing.
 */
media_source_t media_source_current(void);

/**
 * Source-aware transport dispatcher shared by the on-device UI (overlay,
 * touch hotspots) and remote clients (WS plain commands, MQTT). Radio
 * next/prev wrap around the playlist; BT volume uses its own channel.
 */
void media_control_execute(media_source_t source, media_action_t action);

/**
 * Explicit source switch ("switch to radio/SD/BT" voice command). No-op when
 * the target is already playing. The services handle the mux themselves:
 * starting the radio or the SD player takes the output over from the other
 * sources; BT is enabled volatile (no SPIFFS write — safe on shallow
 * httpd/mqtt task stacks) and told to play.
 */
void media_source_switch(media_source_t target);

/**
 * Executes one plain-text command ("next", "vol=42", "playstation=3", …) —
 * the shared vocabulary documented in docs/ws_protocol.md.
 *
 * Transport-independent on purpose: the WS server and the ESP-NOW link both
 * strip their own framing and land here, so the vocabulary has exactly one
 * implementation and cannot drift between the two links.
 *
 * Unknown commands are logged and dropped; there is no reply and no error
 * signal, matching the fire-and-forget contract.
 */
void media_command_execute_text(const char *cmd);

#ifdef __cplusplus
}
#endif
