#pragma once

#include <stdbool.h>

// Invoked from the BT UART task when the phone's play state changes:
// playing=true on BT_PA, playing=false on pause/stop (+SRC=NONE) or disconnect.
// Registered by a higher layer (radio_service) to enforce exclusive source.
typedef void (*bt_play_event_cb_t)(bool playing);

void bt_init(void);
void bt_set_enabled(bool enabled);
bool bt_get_enabled(void);
void bt_set_volume(int volume);
int bt_get_volume();
void bt_set_vol_sync(bool on);       // sync module volume with the phone on/off
void bt_play(void);                  // tell the phone to resume playback
void bt_pause(void);                 // tell the phone to pause playback
void bt_next(void);                  // skip to next track
void bt_prev(void);                  // skip to previous track
void bt_reboot(void);                // reboot module so pending config applies now
void bt_check_connection();
void bt_send_raw(const char *cmd);
void bt_set_play_event_cb(bt_play_event_cb_t cb);