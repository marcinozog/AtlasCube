#pragma once

#include <stdbool.h>

void bt_init(void);
void bt_set_enabled(bool enabled);
bool bt_get_enabled(void);
void bt_set_volume(int volume);
int bt_get_volume();
void bt_check_connection();
void bt_send_raw(const char *cmd);