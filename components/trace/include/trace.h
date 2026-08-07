#pragma once

#include "esp_log.h"
#include "esp_timer.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Runtime-gated diagnostic logging.
//
// ESP_LOGD/ESP_LOGV are compiled out of a release build — CONFIG_LOG_MAXIMUM_LEVEL
// follows the default level (INFO), so esp_log_level_set() cannot bring them back
// on a device in the field, and raising the maximum would drag the debug strings
// of every IDF component into flash for the sake of a handful of our own lines.
//
// These macros keep the diagnostic output at INFO level and gate it on a runtime
// bitmask instead: nothing but our own call sites costs flash, the switch is
// per-subsystem, and a tester can arm it from the web UI on a stock release
// binary. The mask is persisted (settings section "trace"), so arming a flag and
// restarting also captures the boot sequence.
typedef enum {
    TRACE_TOUCH   = 1u << 0,   // touch controller: raw readings + mapped pixels
    TRACE_DISPLAY = 1u << 1,   // LVGL flush geometry and rate
    TRACE_AUDIO   = 1u << 2,   // media metadata parsing
    TRACE_WEB     = 1u << 3,   // HTTP file serving + internal-heap headroom
    TRACE_ESPNOW  = 1u << 4,   // ESP-NOW frame handling
} trace_flag_t;

// Read straight from the macros below; written only by trace_set_mask().
extern volatile uint32_t g_trace_mask;

static inline bool trace_on(uint32_t flag) { return (g_trace_mask & flag) != 0; }

// One trace line. Emitted at INFO so it reaches a stock release build; the tag
// is explicit because not every caller keeps its tag in a `TAG` symbol.
#define TRACE(flag, tag, fmt, ...) \
    do { if (trace_on(flag)) ESP_LOGI(tag, fmt, ##__VA_ARGS__); } while (0)

// Same, but at most once per `ms` at this call site — for hot paths (flush
// callbacks, touch polling) where an unthrottled line would drown the console
// and distort the very timing it is there to measure. The throttle state is a
// function-local static, so two call sites never share a budget.
// 0 doubles as "nothing logged here yet" so the first hit is never swallowed —
// esp_timer_get_time() only returns 0 in the first microsecond after boot, well
// before any of these call sites can run.
#define TRACE_EVERY_MS(flag, tag, ms, fmt, ...)                          \
    do {                                                                 \
        if (trace_on(flag)) {                                            \
            static int64_t _trace_last_us = 0;                           \
            int64_t _trace_now = esp_timer_get_time();                   \
            if (_trace_last_us == 0 ||                                   \
                _trace_now - _trace_last_us >= (int64_t)(ms) * 1000) {   \
                _trace_last_us = _trace_now;                             \
                ESP_LOGI(tag, fmt, ##__VA_ARGS__);                       \
            }                                                            \
        }                                                                \
    } while (0)

// Replaces the whole mask (0 = quiet). Logs the new state unconditionally: a
// device that suddenly starts — or stops — talking should say why.
void     trace_set_mask(uint32_t mask);
uint32_t trace_get_mask(void);

// Flag table. Settings and the REST layer iterate it instead of hard-coding
// names, so a new flag is one line in trace.c and one TRACE() at the call site —
// it then shows up in config JSON and in the web UI by itself.
int         trace_flag_count(void);
const char *trace_flag_name(int i);   // NULL when out of range
uint32_t    trace_flag_bit(int i);    // 0 when out of range

#ifdef __cplusplus
}
#endif
