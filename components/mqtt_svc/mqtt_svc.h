#pragma once

#include "sdkconfig.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_MQTT_ENABLE

void mqtt_svc_init(void);

void mqtt_svc_reconfigure(void);

bool mqtt_svc_is_connected(void);

void mqtt_svc_publish_widget_bool(int widget_idx, bool on);
void mqtt_svc_publish_widget_int (int widget_idx, int value);

typedef void (*mqtt_svc_widget_state_cb_t)(int widget_idx, const char *value);
void mqtt_svc_set_widget_state_cb(mqtt_svc_widget_state_cb_t cb);

typedef void (*mqtt_svc_ss_state_cb_t)(const char *value);
void mqtt_svc_set_ss_state_cb(mqtt_svc_ss_state_cb_t cb);

#else  /* !CONFIG_MQTT_ENABLE */

static inline void mqtt_svc_init(void)                                  {}
static inline void mqtt_svc_reconfigure(void)                           {}
static inline bool mqtt_svc_is_connected(void)                          { return false; }
static inline void mqtt_svc_publish_widget_bool(int i, bool on)         { (void)i; (void)on; }
static inline void mqtt_svc_publish_widget_int (int i, int v)           { (void)i; (void)v; }
typedef void (*mqtt_svc_widget_state_cb_t)(int widget_idx, const char *value);
static inline void mqtt_svc_set_widget_state_cb(mqtt_svc_widget_state_cb_t cb) { (void)cb; }
typedef void (*mqtt_svc_ss_state_cb_t)(const char *value);
static inline void mqtt_svc_set_ss_state_cb(mqtt_svc_ss_state_cb_t cb) { (void)cb; }

#endif

#ifdef __cplusplus
}
#endif
