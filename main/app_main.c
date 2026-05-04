#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_app_desc.h"
#include "nvs_flash.h"
#include "playlist.h"
#include "audio_player.h"
#include "http_server.h"
#include "wifi_manager.h"
#include "radio_service.h"
#include "ws_server.h"
#include "esp_spiffs.h"
#include "display.h"
#include "settings.h"
#include "app_state.h"
#include "bt.h"
#include "ntp_service.h"
// #include "ui_events.h"
#include "ui_manager.h"
#include "defines.h"
#include "encoder.h"
#include "buzzer.h"
#include "events_service.h"
#include "ui_profile.h"

static const char *TAG = "MAIN";

void init_fs(void);
void system_monitor_task(void *pv);

void app_main(void)
{
    const esp_app_desc_t *app_desc = esp_app_get_description();
    ESP_LOGI(TAG, "AtlasCube firmware %s (built %s %s, IDF %s)",
             app_desc->version, app_desc->date, app_desc->time, app_desc->idf_ver);

    nvs_flash_init();
    init_fs();

    // ── Basic initialization (order matters!) ────────────────────────────────
    app_state_init();      // 1. initializes s_cbs[]
    bt_init();             // 2. no subscribe
    settings_init();       // 3. no subscribe
    buzzer_init(BUZZER_GPIO);
    events_service_init();
    ui_profile_load_from_file();   // layout overrides — must run before display_init()
    display_init();        // 4. ui_manager_init() → subscribe #1
    encoder_init();

    // ── WiFi ──────────────────────────────────────────────────────────────────
    // If wifi.ssid is empty (first boot or missing file) → AP-only.
    // If ssid is saved → STA attempt for 15s, fallback to AP.
    app_settings_t *s = settings_get();
    wifi_init(s->wifi.ssid, s->wifi.password); // AP if ssid empty or STA fail

    // ── Services requiring internet ──────────────────────────────────────────
    if (wifi_get_run_mode() == WIFI_RUN_MODE_STA) {
        ntp_service_init();
        // Other online services can be started here, e.g. weather
    } else {
        ESP_LOGW(TAG, "AP mode — NTP and radio unavailable until WiFi is configured");
    }

    // ── Audio (initialization independent of WiFi) ───────────────────────────
    // audio_player and radio_service only initialize structures/pipeline;
    // actual streaming happens only after calling play, which requires STA.
    audio_player_init();
    playlist_load();
    radio_service_init();

    // ── Web / WebSocket ───────────────────────────────────────────────────────
    ws_init();          // app_state_subscribe #2
    http_server_start();// available on router IP (STA) and 192.168.4.1 (AP)

    // ── Apply settings to hardware (after UI and WS subscriptions!) ──────────
    // app_state_update() inside settings_apply() will notify all
    // subscribers (#1 UI, #2 WS) — they must already be registered.
    settings_apply();

    // ── Splash screen: wait at least 1500 ms from boot, then proceed ─────────
    #define SPLASH_MIN_MS 3000
    int64_t elapsed_ms = esp_timer_get_time() / 1000;
    if (elapsed_ms < SPLASH_MIN_MS) {
        vTaskDelay(pdMS_TO_TICKS(SPLASH_MIN_MS - elapsed_ms));
    }
    if (wifi_get_run_mode() == WIFI_RUN_MODE_AP) {
        // Navigate without persisting — SCREEN_WIFI is a forced state, not a user preference.
        // Saving it would cause the device to show WiFi setup after a successful STA reconnect.
        ui_navigate(SCREEN_WIFI);
    } else {
        settings_set_screen(s->display.screen);
    }

    ESP_LOGI(TAG, "System ready. WiFi mode: %s",
             wifi_get_run_mode() == WIFI_RUN_MODE_STA ? "STA" : "AP @ 192.168.4.1");
    ESP_LOGI(TAG, "app_main stack watermark: %u bytes",
             uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t));
             
    // xTaskCreate(system_monitor_task, "sys_mon", 4096, NULL, 5, NULL);
}

void init_fs(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 5,
        .format_if_mount_failed = true
    };

    esp_vfs_spiffs_register(&conf);

    size_t total = 0, used = 0;
    esp_spiffs_info("storage", &total, &used);
    ESP_LOGI("SPIFFS", "Total: %d, Used: %d", total, used);
}

void system_monitor_task(void *pv)
{
    while (1) {
        ESP_LOGI(TAG, "=== SYSTEM ===");

        ESP_LOGI(TAG, "Free heap: %d", esp_get_free_heap_size());
        ESP_LOGI(TAG, "Min heap: %d", esp_get_minimum_free_heap_size());
        ESP_LOGI(TAG, "Internal RAM free: %d", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
        ESP_LOGI(TAG, "PSRAM free: %d", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

        char stats[1024];
        vTaskGetRunTimeStats(stats);
        printf("%s\n", stats);

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}