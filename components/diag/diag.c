#include "diag.h"
#include "sdcard.h"
#include "updater.h"
#include "wifi_manager.h"
#include "esp_app_desc.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "esp_vfs_fat.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "driver/temperature_sensor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "DIAG";

// NULL until diag_init() succeeds; every read is guarded, so a board whose
// sensor refuses to install just reports no temperature.
static temperature_sensor_handle_t s_tsens = NULL;

static const char *chip_model_name(esp_chip_model_t m)
{
    switch (m) {
        case CHIP_ESP32:   return "ESP32";
        case CHIP_ESP32S2: return "ESP32-S2";
        case CHIP_ESP32S3: return "ESP32-S3";
        case CHIP_ESP32C3: return "ESP32-C3";
        case CHIP_ESP32C6: return "ESP32-C6";
        case CHIP_ESP32H2: return "ESP32-H2";
        default:           return "unknown";
    }
}

void diag_init(void)
{
    if (s_tsens) return;

    // -10..80 °C is the range with the tightest error (±1 °C). A die that ever
    // leaves it reports a clamped value, which is still the right message.
    temperature_sensor_config_t cfg = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);
    esp_err_t err = temperature_sensor_install(&cfg, &s_tsens);
    if (err == ESP_OK) err = temperature_sensor_enable(s_tsens);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "temperature sensor unavailable: %s", esp_err_to_name(err));
        if (s_tsens) { temperature_sensor_uninstall(s_tsens); s_tsens = NULL; }
        return;
    }
    // Left enabled for the lifetime of the device: the sensor draws next to
    // nothing and enable/disable around every 1 Hz read would only add jitter.
}

void diag_collect(diag_info_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));

    /* ── firmware ── */
    const esp_app_desc_t *app = esp_app_get_description();
    snprintf(out->fw_version, sizeof(out->fw_version), "%s", app->version);
    snprintf(out->fw_build,   sizeof(out->fw_build),   "%s %s", app->date, app->time);
    snprintf(out->idf_version, sizeof(out->idf_version), "%s", app->idf_ver);
    out->fw_variant      = app_fw_variant();
    out->www_outdated    = updater_www_outdated();
    out->update_available = updater_update_available();
    snprintf(out->update_latest, sizeof(out->update_latest), "%s", updater_latest_version());

    /* ── memory ── */
    out->psram_total    = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    out->psram_free     = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    out->psram_min_free = heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM);
    out->int_total      = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    out->int_free       = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    out->int_min_free   = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
    out->int_largest    = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    /* ── hardware ── */
    esp_chip_info_t chip;
    esp_chip_info(&chip);
    snprintf(out->chip, sizeof(out->chip), "%s", chip_model_name(chip.model));
    out->chip_rev   = chip.revision;
    out->chip_cores = chip.cores;
    uint32_t flash = 0;
    if (esp_flash_get_size(NULL, &flash) == ESP_OK) out->flash_size = flash;
    float tc = 0.0f;
    if (s_tsens && temperature_sensor_get_celsius(s_tsens, &tc) == ESP_OK) {
        out->temp_valid = true;
        out->temp_c10   = (int)lroundf(tc * 10.0f);
    }

    /* ── link ── */
    wifi_ap_record_t ap;
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
        out->wifi_connected = true;
        out->rssi = ap.rssi;
        snprintf(out->ssid, sizeof(out->ssid), "%s", (const char *)ap.ssid);
    }
    wifi_get_ip(out->ip, sizeof(out->ip));
    uint8_t mac[6] = {0};
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK) {
        snprintf(out->mac, sizeof(out->mac), "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    /* ── storage ── */
    out->sd_mounted = sdcard_is_mounted();
    if (out->sd_mounted) {
        uint64_t total = 0, freeb = 0;
        if (esp_vfs_fat_info(SD_MOUNT_POINT, &total, &freeb) == ESP_OK) {
            out->sd_total = total;
            out->sd_free  = freeb;
        }
    }

    out->uptime_s = (uint32_t)(esp_timer_get_time() / 1000000);
}

void diag_cpu_usage(diag_cpu_state_t *st, int *cpu0, int *cpu1)
{
    *cpu0 = *cpu1 = 0;
    if (!st) return;

    UBaseType_t n = uxTaskGetNumberOfTasks();
    if (n == 0) return;

    TaskStatus_t *array = pvPortMalloc(n * sizeof(TaskStatus_t));
    if (!array) return;

    uint32_t total_rt = 0;
    n = uxTaskGetSystemState(array, n, &total_rt);

    TaskHandle_t idle0 = xTaskGetIdleTaskHandleForCore(0);
    TaskHandle_t idle1 = xTaskGetIdleTaskHandleForCore(1);

    uint32_t idle_rt[2] = {0, 0};
    for (UBaseType_t i = 0; i < n; i++) {
        if      (array[i].xHandle == idle0) idle_rt[0] = array[i].ulRunTimeCounter;
        else if (array[i].xHandle == idle1) idle_rt[1] = array[i].ulRunTimeCounter;
    }
    vPortFree(array);

    // First call has no reference yet — return 0 and remember the values.
    if (st->last_total != 0) {
        uint32_t total_delta = total_rt - st->last_total;
        if (total_delta > 0) {
            uint32_t idle0_delta = idle_rt[0] - st->last_idle[0];
            uint32_t idle1_delta = idle_rt[1] - st->last_idle[1];
            int p0 = 100 - (int)((idle0_delta * 100) / total_delta);
            int p1 = 100 - (int)((idle1_delta * 100) / total_delta);
            if (p0 < 0)   p0 = 0;
            if (p0 > 100) p0 = 100;
            if (p1 < 0)   p1 = 0;
            if (p1 > 100) p1 = 100;
            *cpu0 = p0;
            *cpu1 = p1;
        }
    }

    st->last_total   = total_rt;
    st->last_idle[0] = idle_rt[0];
    st->last_idle[1] = idle_rt[1];
}
