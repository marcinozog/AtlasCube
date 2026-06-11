#pragma once

#include <stdbool.h>

// Invoked from the BT UART task when the phone starts playing (BT_PA).
// Registered by a higher layer (radio_service) to enforce exclusive source.
typedef void (*bt_play_event_cb_t)(void);

void bt_init(void);
void bt_set_enabled(bool enabled);
bool bt_get_enabled(void);
void bt_set_volume(int volume);
int bt_get_volume();
void bt_pause(void);                 // tell the phone to pause playback (AT+PU)
void bt_check_connection();
void bt_send_raw(const char *cmd);
void bt_set_play_event_cb(bt_play_event_cb_t cb);