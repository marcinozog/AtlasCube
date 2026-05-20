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

void mqtt_svc_publish_toggle(bool on);

typedef void (*mqtt_svc_toggle_state_cb_t)(bool on);
void mqtt_svc_set_toggle_state_cb(mqtt_svc_toggle_state_cb_t cb);

#else  /* CONFIG_MQTT_ENABLE */

static inline void mqtt_svc_init(void)                                  {}
static inline void mqtt_svc_reconfigure(void)                           {}
static inline bool mqtt_svc_is_connected(void)                          { return false; }
static inline void mqtt_svc_publish_toggle(bool on)                     { (void)on; }
typedef void (*mqtt_svc_toggle_state_cb_t)(bool on);
static inline void mqtt_svc_set_toggle_state_cb(mqtt_svc_toggle_state_cb_t cb) { (void)cb; }

#endif /* CONFIG_MQTT_ENABLE */

#ifdef __cplusplus
}
#endif
