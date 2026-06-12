#include "defines.h"
#include "esp_log.h"
#include "esp_app_desc.h"
#include "nvs_flash.h"
#include "playlist.h"
#include "audio_player.h"
#include "http_server.h"
#include "wifi_manager.h"
#include "mdns_service.h"
#include "ws_server.h"
#include "esp_spiffs.h"
#include "sdcard.h"
#include "display.h"
#include "settings.h"
#include "bt.h"
#include "ntp_service.h"
#include "ui_manager.h"
#include "encoder.h"
#include "touch.h"
#include "buzzer.h"
#include "rgb_led.h"
#include "battery.h"
#include "events_service.h"
#include "dim_schedule.h"
#include "radio_service.h"
#include "ui_profile.h"
#include "mqtt_svc.h"
#include "mqtt_config.h"

static const char *TAG = "MAIN";

// Forward-declared from ESP-ADF's audio_board (components/audio_board/
// esp32_s3_atlascube/board.h). We can't include board.h directly here:
// ADF's audio_board CMakeLists.txt puts audio_hal in PRIV_REQUIRES, so
// board.h's transitive #include "audio_hal.h" only resolves *inside* the
// audio_board component. We only need the side effects (codec config +
// PA enable on ES3C28P) and discard the returned handle, so an opaque
// forward declaration is enough — the linker resolves the symbol via
// `audio_board` in main/CMakeLists.txt REQUIRES.
typedef struct audio_board_handle *audio_board_handle_t;
audio_board_handle_t audio_board_init(void);

void init_fs(void);
void system_monitor_task(void *pv);

void app_main(void)
{
    const esp_app_desc_t *app_desc = esp_app_get_description();
    ESP_LOGI(TAG, "AtlasCube firmware %s (built %s %s, IDF %s)",
             app_desc->version, app_desc->date, app_desc->time, app_desc->idf_ver);

    nvs_flash_init();
    init_fs();
#ifdef HAS_SD_CARD
    sdcard_init();   // non-fatal — a missing/failed card only logs a warning
#endif

    // mqtt config must load after fs is mounted; before http_server starts
    // so /api/mqtt GET sees real data. mqtt_svc_init() also calls load again
    // (idempotent) so it's safe to read here for early consumers.
    mqtt_config_load();

    // ── Basic initialization (order matters!) ────────────────────────────────
    app_state_init();      // 1. initializes s_cbs[]
    bt_init();             // 2. no subscribe
    settings_init();       // 3. no subscribe
    buzzer_init(BUZZER_PIN);
    // RGB LED + battery monitor — both return ESP_ERR_NOT_SUPPORTED with a
    // log line on boards where the feature isn't wired (RGB_LED_PIN/BAT_ADC_PIN
    // == -1), so the calls are safe regardless of variant.
    rgb_led_init();
    battery_init();
    events_service_init();
    ui_profile_load_from_file();   // layout overrides — must run before display_init()
    display_init();        // 4. ui_manager_init() → subscribe #1
    encoder_init();
    // audio_board_init() MUST run before touch_init() on ES3C28P:
    //   - ADF v2.8 audio_hal_init → es8311 → i2c_new_master_bus(I2C_NUM_0)
    //     creates the shared I2C bus (SCL=15, SDA=16) and configures the
    //     ES8311 codec. It also drives SC8002B PA enable (GPIO1) LOW.
    //   - touch_init() then calls i2c_master_get_bus_handle() to attach
    //     FT6336U as another device on the same bus.
    //   - Reverse order would crash: touch_init would create a legacy-API
    //     bus first, ADF's new-API bus_create would then abort with
    //     "CONFLICT! driver_ng is not allowed to be used with this old driver".
    //   - On AtlasCube this is a near-noop (no codec, no I2C bus from ADF)
    //     and touch_init() falls back to creating the bus itself on
    //     CTP_SCL=47 / CTP_SDA=48.
    audio_board_init();
    touch_init();          // registers LVGL indev — must run before display_start()
    display_start();       // starts lvgl_task; no LVGL state may be touched from other tasks afterwards
    dim_schedule_init();   // periodic check of dim/bright window; no-op until NTP syncs

    // ── WiFi ──────────────────────────────────────────────────────────────────
    // If wifi.ssid is empty (first boot or missing file) → AP-only.
    // If ssid is saved → STA attempt for 15s, fallback to AP.
    app_settings_t *s = settings_get();
    wifi_init(s->wifi.ssid, s->wifi.password); // AP if ssid empty or STA fail

    // ── Services requiring internet ──────────────────────────────────────────
    if (wifi_get_run_mode() == WIFI_RUN_MODE_STA) {
        ntp_service_init();
        mdns_service_start();   // <hostname>.local — STA only (AP IP is fixed)
        // Other online services can be started here, e.g. weather
    } else {
        ESP_LOGW(TAG, "AP mode — NTP and radio unavailable until WiFi is configured");
    }

    // ── Audio (initialization independent of WiFi) ───────────────────────────
    // audio_player and radio_service only initialize structures/pipeline;
    // actual streaming happens only after calling play, which requires STA.
    // audio_board_init() already ran above (had to be before touch_init).
    audio_player_init();
    playlist_load();
    radio_service_init();

    // ── Web / WebSocket ───────────────────────────────────────────────────────
    ws_init();          // app_state_subscribe #2
    http_server_start();// available on router IP (STA) and 192.168.4.1 (AP)

    // ── MQTT ─────────────────────────────────────────────────────────────────
    // Only meaningful in STA (broker is on the LAN). esp-mqtt has its own
    // reconnect loop, so we just start it once.
    if (wifi_get_run_mode() == WIFI_RUN_MODE_STA) {
        mqtt_svc_init();
    }

    // ── Apply settings to hardware (after UI and WS subscriptions!) ──────────
    // app_state_update() inside settings_apply() will notify all
    // subscribers (#1 UI, #2 WS) — they must already be registered.
    settings_apply();

    // ── Resume radio if it was playing before the last reboot ────────────────
    // Opt-in via playlist.resume_on_boot. STA only — radio needs internet.
    if (wifi_get_run_mode() == WIFI_RUN_MODE_STA) {
        radio_resume_on_boot();
    }

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