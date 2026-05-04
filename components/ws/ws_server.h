#pragma once
#include "esp_http_server.h"

void ws_init(void);
void ws_register(httpd_handle_t server);
void ws_send_bt_log(const char *line);
void ws_on_close(httpd_handle_t hd, int fd);