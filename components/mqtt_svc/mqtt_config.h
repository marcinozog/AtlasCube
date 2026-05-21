#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MQTT_MAX_WIDGETS 6

typedef enum {
    MQTT_W_NONE   = 0,
    MQTT_W_TOGGLE = 1,
    MQTT_W_SLIDER = 2,
    MQTT_W_LABEL  = 3,
} mqtt_widget_type_t;

typedef struct {
    mqtt_widget_type_t type;
    char  title[24];
    char  topic_cmd[96];      // toggle / slider
    char  topic_state[96];    // all (subscribed if non-empty)
    char  json_path[48];      // empty = use whole payload (plain text mode)
    int   min, max, step;     // slider only
    char  unit[8];            // label only (e.g. "°C", "%")
} mqtt_widget_t;

typedef struct {
    bool          enabled;
    char          host[64];
    int           port;
    char          username[32];
    char          password[64];
    char          client_id[32];
    char          base_topic[32];
    mqtt_widget_t widgets[MQTT_MAX_WIDGETS];
} mqtt_config_t;

esp_err_t      mqtt_config_load(void);
esp_err_t      mqtt_config_save(void);
mqtt_config_t *mqtt_config_get(void);

const char *mqtt_widget_type_name(mqtt_widget_type_t t);
mqtt_widget_type_t mqtt_widget_type_from_name(const char *s);

#ifdef __cplusplus
}
#endif
