#include "trace.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "TRACE";

volatile uint32_t g_trace_mask = 0;

// The one place a flag is named. Order is free — the web UI renders whatever
// this table reports, in this order.
static const struct {
    const char *name;
    uint32_t    bit;
} k_flags[] = {
    { "touch",   TRACE_TOUCH   },
    { "display", TRACE_DISPLAY },
    { "audio",   TRACE_AUDIO   },
    { "web",     TRACE_WEB     },
    { "espnow",  TRACE_ESPNOW  },
};

#define FLAG_COUNT ((int)(sizeof(k_flags) / sizeof(k_flags[0])))

void trace_set_mask(uint32_t mask)
{
    if (mask == g_trace_mask) return;
    g_trace_mask = mask;

    char names[128];
    size_t len = 0;
    names[0] = '\0';
    for (int i = 0; i < FLAG_COUNT; i++) {
        if (!(mask & k_flags[i].bit)) continue;
        int n = snprintf(names + len, sizeof(names) - len,
                         len ? " %s" : "%s", k_flags[i].name);
        if (n < 0 || (size_t)n >= sizeof(names) - len) break;
        len += n;
    }
    ESP_LOGI(TAG, "mask 0x%02" PRIx32 " [%s]", mask, len ? names : "quiet");
}

uint32_t trace_get_mask(void) { return g_trace_mask; }

int trace_flag_count(void) { return FLAG_COUNT; }

const char *trace_flag_name(int i)
{
    return (i >= 0 && i < FLAG_COUNT) ? k_flags[i].name : NULL;
}

uint32_t trace_flag_bit(int i)
{
    return (i >= 0 && i < FLAG_COUNT) ? k_flags[i].bit : 0;
}
