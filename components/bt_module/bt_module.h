#pragma once

#include <stdbool.h>

// One Bluetooth module's AT dialect: the commands we send and the substrings we
// look for in its UART replies. The active module is selected at build time via
// the BT_MODULE_* define in defines.h and exposed as `g_bt`. This lets the rest
// of the firmware and the web UI speak in semantic terms (play, pause, set
// volume, sync volume) while every module-specific AT string lives in exactly
// one place — its descriptor. Adding a new module means adding one descriptor
// file, not editing the driver.
typedef struct {
    const char *name;

    // --- transport / control (raw AT, no CRLF; bt_send_raw appends it) ---
    const char *cmd_play;
    const char *cmd_pause;
    const char *cmd_next;
    const char *cmd_prev;
    const char *cmd_get_state;      // query connection state
    const char *cmd_get_meta;       // request title/artist
    const char *cmd_meta_time_on;   // enable playback-time reporting
    const char *cmd_get_codec;      // query current audio codec
    const char *cmd_get_arate;      // query current sample rate / bit depth
    const char *cmd_reboot;         // reboot the module so NVM/ADK config
                                    // (SVSTEP, SYNCVOL...) takes effect now

    // --- volume sync between the module and the phone ---
    const char *cmd_sync_vol_on;
    const char *cmd_sync_vol_off;
    const char *cmd_sync_vol_state; // query sync state (reply shows in console)

    // --- volume scaling ---
    int         vol_max;            // SVOL max (0..vol_max); must equal the step
                                    // count enforced by cmd_set_vstep minus 1
    const char *fmt_set_vol;        // printf fmt with one %d, e.g. "AT+SVOL=%d"
    const char *cmd_set_vstep;      // fixed step count sent at init so the module
                                    // always exposes the same volume resolution
                                    // (e.g. "AT+SVSTEP=128"); applies on reboot

    // --- parser: substrings searched in incoming UART data ---
    const char *evt_connected;      // link up
    const char *evt_connected_alt;  // alternate "connected" string, NULL if none
    const char *evt_disconnected;   // link down
    const char *evt_discoverable;   // pairing / discoverable
    const char *evt_play;           // phone started playback
    const char *evt_src_none;       // source cleared / playback stopped
    const char *key_title;          // prefix before the track title
    const char *key_artist;         // prefix before the artist
    const char *key_dur_ms;         // prefix before total duration (ms)
    const char *key_pos_s;          // prefix before current position (s)
    const char *key_codec;          // prefix before the codec name
    const char *key_arate;          // prefix before the sample rate (Hz)
    const char *key_abit;           // prefix before the bit depth
} bt_module_desc_t;

// The module selected via BT_MODULE_* in defines.h. Defined in exactly one
// descriptor source file; a link error here means no module was selected.
extern const bt_module_desc_t *const g_bt;
