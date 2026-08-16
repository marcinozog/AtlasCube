#include "defines.h"
#include "esp_log.h"
#include "esp_app_desc.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "playlist.h"
#include "audio_engine.h"
#include "audio_net_player.h"
#include "audio_file_player.h"
#include "http_server.h"
#include "wifi_manager.h"
#include "network_services.h"
#include "ws_server.h"
#include "esp_spiffs.h"
#include "sdcard.h"
#include "display.h"
#include "settings.h"
#include "bt.h"
#include "ui_manager.h"
#include "encoder.h"
#include "touch.h"
#include "buzzer.h"
#include "events_service.h"
#include "dim_schedule.h"
#include "radio_service.h"
#include "ui_profile.h"
#include "espnow_link.h"
#include "mqtt_config.h"
#include "heap_report.h"
#include "board_pins.h"
#include "diag.h"

static const char *TAG = "MAIN";

void init_fs(void);
void system_monitor_task(void *pv);

void app_main(void)
{
    const esp_app_desc_t *app_desc = esp_app_get_description();
    ESP_LOGI(TAG, "AtlasCube firmware %s (built %s %s, IDF %s)",
             app_desc->version, app_desc->date, app_desc->time, app_desc->idf_ver);

    nvs_flash_init();
    diag_init();         // die temperature sensor — must own it before any reader polls
    board_pins_load();   // resolve runtime pin map (defaults + NVS overrides) before any peripheral init
    init_fs();
    // SD is mounted lazily on first use (sdcard_init from the SD player / file
    // manager / photo screensaver / voice events), not at boot — a radio-only
    // session never pays the SDMMC+FATFS internal-RAM cost.

    // mqtt config must load after fs is mounted; before http_server starts
    // so /api/mqtt GET sees real data. mqtt_svc_init() also calls load again
    // (idempotent) so it's safe to read here for early consumers.
    mqtt_config_load();

    // ── Basic initialization (order matters!) ────────────────────────────────
    app_state_init();      // 1. initializes s_cbs[]
    bt_init();             // 2. no subscribe
    settings_init();       // 3. no subscribe
    buzzer_init(g_pins.buzzer);
    events_service_init();
    ui_profile_load_from_file();   // layout overrides — must run before display_init()
    display_init();        // 4. ui_manager_init() → subscribe #1
    encoder_init();
    touch_init();          // registers LVGL indev — must run before display_start()
    display_start();       // starts lvgl_task; no LVGL state may be touched from other tasks afterwards
    dim_schedule_init();   // periodic check of dim/bright window; no-op until NTP syncs

    // ── WiFi ──────────────────────────────────────────────────────────────────
    // If wifi.ssid is empty (first boot or missing file) → AP-only.
    // If ssid is saved → STA attempt for 15s, fallback to AP. A failed attempt
    // is not final: a supervisor task keeps retrying in the background and the
    // device moves to STA on its own whenever the router turns up.
    app_settings_t *s = settings_get();
    wifi_init(s->wifi.ssid, s->wifi.password); // AP if ssid empty or STA fail

    if (wifi_get_run_mode() != WIFI_RUN_MODE_STA)
        ESP_LOGW(TAG, "AP mode — NTP and radio unavailable until WiFi connects");

    // Everything that needs the network now lives in network_services_start()
    // below, driven by the link callback — see main/network_services.h.

    // ── Audio (initialization independent of WiFi) ───────────────────────────
    // The audio engine and radio_service only initialize structures/pipeline;
    // actual streaming happens only after calling play, which requires STA.
    // Engine first (creates the pipeline + tasks), then the source layers
    // (net/file only register their hooks with the engine).
    audio_engine_init();
    audio_net_player_init();
    audio_file_player_init();
    playlist_load();
    radio_service_init();

    // ── Web / WebSocket ───────────────────────────────────────────────────────
    ws_init();          // app_state_subscribe #2
    http_server_start();// available on router IP (STA) and 192.168.4.1 (AP)

    // ── ESP-NOW remote link ───────────────────────────────────────────────────
    // Third control transport, sharing the WS command vocabulary — see
    // docs/espnow_link.md. Runs in both STA and AP mode (it binds to whichever
    // interface is up), and after playlist_load() so get_list has data.
    espnow_link_init();

    // ── Boot-info splash hold ────────────────────────────────────────────────
    // The splash is still the active screen — settings_apply() below is what
    // navigates away. In STA, hold it so the version + IP overlay
    // (screen_splash.c) is readable with the real lease before we leave it.
    // Only the boot-time outcome counts here: a device still in AP must not stall
    // the splash waiting for a recovery that may take minutes.
    #define BOOT_INFO_MS 3000
    if (wifi_get_run_mode() == WIFI_RUN_MODE_STA
        && settings_get()->display.show_boot_info) {
        vTaskDelay(pdMS_TO_TICKS(BOOT_INFO_MS));
    }

    // ── Apply settings to hardware (after UI and WS subscriptions!) ──────────
    // app_state_update() inside settings_apply() will notify all subscribers
    // (#1 UI, #2 WS) — they must already be registered. It also navigates to the
    // saved screen, which is what ends the splash.
    settings_apply();

    // ── Splash minimum on-screen time ────────────────────────────────────────
    // settings_apply() above already navigated to the saved screen; this is just
    // the original floor on total splash time for the fast-boot / AP path.
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

    // ── Online services ──────────────────────────────────────────────────────
    // Last, because everything it starts assumes the UI, WS, HTTP server and
    // ESP-NOW are already up. Runs them now if the link is up, otherwise on
    // whichever later link-up the recovery supervisor achieves.
    network_services_start();

    ESP_LOGI(TAG, "app_main stack watermark: %u bytes",
             uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t));

    heap_report("boot-done");   // internal-RAM baseline before any stream

    // Diagnostic (normally off): per-task CPU% every 5 s — attributes per-core
    // load, e.g. when a heavy stream starves the UI. Uncomment to investigate.
    // cpu_report_start();

    // xTaskCreate(system_monitor_task, "sys_mon", 4096, NULL, 5, NULL);
}

void init_fs(void)
{
    // Two SPIFFS partitions (see partitions16MB.csv):
    //   www    → /spiffs : editable web UI, may be wiped/re-uploaded at runtime.
    //   config → /config : user data (settings JSON + station list playlist.csv),
    //                       physically isolated so a www update can never clobber it.
    esp_vfs_spiffs_conf_t www_conf = {
        .base_path = "/spiffs",
        .partition_label = "www",
        .max_files = 5,
        .format_if_mount_failed = true
    };
    esp_vfs_spiffs_register(&www_conf);

    esp_vfs_spiffs_conf_t config_conf = {
        .base_path = "/config",
        .partition_label = "config",
        .max_files = 5,
        .format_if_mount_failed = true
    };
    esp_vfs_spiffs_register(&config_conf);

    size_t total = 0, used = 0;
    esp_spiffs_info("www", &total, &used);
    ESP_LOGI("SPIFFS", "www:    Total: %d, Used: %d", total, used);
    esp_spiffs_info("config", &total, &used);
    ESP_LOGI("SPIFFS", "config: Total: %d, Used: %d", total, used);
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
