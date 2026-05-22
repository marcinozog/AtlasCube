#include "dim_schedule.h"

#include "display.h"
#include "settings.h"
#include "ntp_service.h"

#include "esp_log.h"
#include "esp_timer.h"

#include <time.h>

static const char *TAG = "DIM_SCHED";

#define TICK_US (30 * 1000 * 1000ULL)

typedef enum {
    STATE_UNKNOWN = -1,
    STATE_BRIGHT  = 0,
    STATE_DIM     = 1,
} dim_state_t;

static esp_timer_handle_t s_timer;
static bool               s_initialized = false;
static dim_state_t        s_last_state  = STATE_UNKNOWN;
static bool               s_last_enabled = false;

// True if "now" (in minutes since midnight) falls inside [dim, bright) window,
// handling the common wrap-around case (dim=22:00, bright=07:00).
static bool in_dim_window(int now_min, int dim_min, int bright_min)
{
    if (dim_min == bright_min) return false;   // zero-length window → never dim
    if (dim_min < bright_min) {
        return now_min >= dim_min && now_min < bright_min;
    }
    // wraps past midnight
    return now_min >= dim_min || now_min < bright_min;
}

// Re-apply backlight only on transitions (window enter/exit) — this way a
// manual brightness change made during the dim window holds until the next
// boundary instead of being overwritten on the next 30 s tick.
static void evaluate(void)
{
    if (!ntp_service_is_synced()) return;

    app_settings_t *s = settings_get();
    const dim_schedule_t *ds = &s->display.dim_schedule;

    // Schedule turned off → if it was on before, restore "day" brightness once.
    if (!ds->enabled) {
        if (s_last_enabled) {
            display_set_backlight((uint8_t)s->display.brightness);
            ESP_LOGI(TAG, "Schedule disabled → restoring day brightness=%d",
                     s->display.brightness);
        }
        s_last_enabled = false;
        s_last_state   = STATE_UNKNOWN;
        return;
    }

    time_t now = time(NULL);
    struct tm lt;
    localtime_r(&now, &lt);
    int now_min    = lt.tm_hour * 60 + lt.tm_min;
    int dim_min    = ds->dim_hour    * 60 + ds->dim_minute;
    int bright_min = ds->bright_hour * 60 + ds->bright_minute;

    dim_state_t cur = in_dim_window(now_min, dim_min, bright_min) ? STATE_DIM : STATE_BRIGHT;

    bool fresh = !s_last_enabled || s_last_state == STATE_UNKNOWN;
    if (fresh || cur != s_last_state) {
        int target = (cur == STATE_DIM) ? ds->dim_brightness : s->display.brightness;
        ESP_LOGI(TAG, "%s → brightness=%d (now=%02d:%02d, dim=%02d:%02d, bright=%02d:%02d)",
                 fresh ? "Init" : (cur == STATE_DIM ? "Enter dim" : "Exit dim"),
                 target, lt.tm_hour, lt.tm_min,
                 ds->dim_hour, ds->dim_minute,
                 ds->bright_hour, ds->bright_minute);
        display_set_backlight((uint8_t)target);
        s_last_state = cur;
    }
    s_last_enabled = true;
}

static void timer_cb(void *arg)
{
    (void)arg;
    evaluate();
}

esp_err_t dim_schedule_init(void)
{
    if (s_initialized) return ESP_OK;

    esp_timer_create_args_t args = {
        .callback = timer_cb,
        .name     = "dim_sched_tick",
    };
    esp_err_t err = esp_timer_create(&args, &s_timer);
    if (err != ESP_OK) return err;

    err = esp_timer_start_periodic(s_timer, TICK_US);
    if (err != ESP_OK) return err;

    s_initialized = true;
    ESP_LOGI(TAG, "Initialized, tick=%llu us", (unsigned long long)TICK_US);

    evaluate();
    return ESP_OK;
}

void dim_schedule_apply_now(void)
{
    if (!s_initialized) return;
    // Force re-evaluation as if it was the first run — this re-applies the
    // currently-correct brightness even if the window state didn't change
    // (e.g. user just edited dim_brightness for the active night window).
    s_last_state   = STATE_UNKNOWN;
    s_last_enabled = false;
    evaluate();
}
