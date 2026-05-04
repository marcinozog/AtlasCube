#include "defines.h"
#include "ntp_service.h"
#include "app_state.h"
#include "ui_manager.h"
#include "ui_events.h"

#include "esp_sntp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <time.h>
#include <string.h>
#include <stdbool.h>

static const char *TAG = "NTP";
static bool s_synced = false;
static bool s_initialized = false;


static void sntp_sync_cb(struct timeval *tv)
{
    (void)tv;

    char buf[32];
    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &t);
    ESP_LOGI(TAG, "Synchronized: %s", buf);

    s_synced = true;

    app_state_update(&(app_state_patch_t){
        .has_time_synced = true,
        .time_synced     = true,
    });

    ui_event_t ev = { .type = UI_EVT_STATE_CHANGED };
    ui_event_send(&ev);
}

void ntp_service_init(void)
{
    setenv("TZ", DEFAULT_TZ_STRING, 1);
    tzset();

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, DEFAULT_NTP_SERVER1);
    esp_sntp_setservername(1, DEFAULT_NTP_SERVER2);

    sntp_set_time_sync_notification_cb(sntp_sync_cb);
    esp_sntp_init();

    s_initialized = true;

    ESP_LOGI(TAG, "SNTP initialized (defaults), waiting for sync...");
}

void ntp_service_reconfigure(const char *server1, const char *server2, const char *tz)
{
    if (!server1 || !server2 || !tz) return;

    ESP_LOGI(TAG, "Reconfigure: %s | %s | TZ=%s", server1, server2, tz);

    // ── 1. TZ update is instant and does not require re-sync ───────────────
    setenv("TZ", tz, 1);
    tzset();

    // If UTC time is already synced, notify the UI immediately.
    // The SNTP callback isn't needed — only the display formatting changed.
    if (s_synced) {
        char buf[32];
        time_t now = time(NULL);
        struct tm t;
        localtime_r(&now, &t);
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &t);
        ESP_LOGI(TAG, "TZ applied, local time: %s", buf);

        app_state_update(&(app_state_patch_t){
            .has_time_synced = true,
            .time_synced     = true,
        });

        ui_event_t ev = { .type = UI_EVT_STATE_CHANGED };
        ui_event_send(&ev);
    }

    // ── 2. Restart SNTP — needed only if the server changed ────────────────
    //    For a TZ-only change this is optional, but kept for consistency.
    if (s_initialized) {
        esp_sntp_stop();
    }

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, server1);
    esp_sntp_setservername(1, server2);
    sntp_set_time_sync_notification_cb(sntp_sync_cb);  // always after stop()
    sntp_set_sync_status(SNTP_SYNC_STATUS_RESET);       // after stop(), before init()
    esp_sntp_init();

    s_initialized = true;
    ESP_LOGI(TAG, "SNTP restarted");
}

bool ntp_service_is_synced(void)
{
    return s_synced;
}