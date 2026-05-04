#include "defines.h"
#include "wifi_manager.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <string.h>

static const char *TAG = "WIFI";

#define WIFI_AP_CHANNEL          1
#define WIFI_AP_MAX_CONN         4
#define WIFI_STA_MAX_RETRY       5
#define WIFI_CONNECT_TIMEOUT_MS  15000

#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1

static EventGroupHandle_t s_event_group;
static bool               s_connected = false;
static int                s_retry_cnt = 0;
static wifi_run_mode_t    s_run_mode  = WIFI_RUN_MODE_AP;

// ─────────────────────────────────────────────────────────────────────────────
static void event_handler(void *arg, esp_event_base_t base,
                           int32_t id, void *data)
{
    if (base == WIFI_EVENT) {
        if (id == WIFI_EVENT_STA_START) {
            esp_wifi_connect();

        } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
            s_connected = false;
            if (s_retry_cnt < WIFI_STA_MAX_RETRY) {
                s_retry_cnt++;
                ESP_LOGI(TAG, "STA retry %d/%d", s_retry_cnt, WIFI_STA_MAX_RETRY);
                esp_wifi_connect();
            } else {
                ESP_LOGW(TAG, "STA: max retries exceeded → FAIL");
                xEventGroupSetBits(s_event_group, WIFI_FAIL_BIT);
            }

        } else if (id == WIFI_EVENT_AP_STACONNECTED) {
            wifi_event_ap_staconnected_t *e = data;
            ESP_LOGI(TAG, "AP: client joined " MACSTR, MAC2STR(e->mac));

        } else if (id == WIFI_EVENT_AP_STADISCONNECTED) {
            wifi_event_ap_stadisconnected_t *e = data;
            ESP_LOGI(TAG, "AP: client left " MACSTR, MAC2STR(e->mac));
        }

    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = data;
        ESP_LOGI(TAG, "STA IP: " IPSTR, IP2STR(&e->ip_info.ip));
        s_connected = true;
        s_retry_cnt = 0;
        xEventGroupSetBits(s_event_group, WIFI_CONNECTED_BIT);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
static void configure_ap(void)
{
    wifi_config_t cfg = {0};
    memcpy(cfg.ap.ssid,     WIFI_AP_SSID, strlen(WIFI_AP_SSID));
    memcpy(cfg.ap.password, WIFI_AP_PASS, strlen(WIFI_AP_PASS));
    cfg.ap.ssid_len       = strlen(WIFI_AP_SSID);
    cfg.ap.channel        = WIFI_AP_CHANNEL;
    cfg.ap.max_connection = WIFI_AP_MAX_CONN;
    cfg.ap.authmode       = WIFI_AUTH_WPA2_PSK;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &cfg));
}

// ─────────────────────────────────────────────────────────────────────────────
void wifi_init(const char *ssid, const char *pass)
{
    s_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, NULL));

    bool has_creds = ssid && ssid[0] != '\0';

    if (has_creds) {
        ESP_LOGI(TAG, "Credentials present → APSTA (STA: \"%s\")", ssid);

        // Create both netifs; AP stays active during the STA attempt
        esp_netif_create_default_wifi_ap();
        esp_netif_create_default_wifi_sta();

        wifi_config_t sta_cfg = {0};
        strncpy((char *)sta_cfg.sta.ssid, ssid, sizeof(sta_cfg.sta.ssid) - 1);
        if (pass && pass[0]) {
            strncpy((char *)sta_cfg.sta.password, pass,
                    sizeof(sta_cfg.sta.password) - 1);
            sta_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
        }

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
        configure_ap();
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));
        ESP_ERROR_CHECK(esp_wifi_start());
        esp_wifi_set_ps(WIFI_PS_NONE);

        EventBits_t bits = xEventGroupWaitBits(
            s_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE, pdFALSE,
            pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT_MS));

        if (bits & WIFI_CONNECTED_BIT) {
            ESP_LOGI(TAG, "STA connected ✓ → disabling AP");
            esp_wifi_set_mode(WIFI_MODE_STA);   // AP interface off
            esp_netif_destroy(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"));
            s_run_mode = WIFI_RUN_MODE_STA;
        } else {
            ESP_LOGW(TAG, "STA timeout/fail → AP mode (192.168.4.1)");
            esp_wifi_disconnect();
            esp_wifi_set_mode(WIFI_MODE_AP);    // STA interface off, AP active
            s_run_mode = WIFI_RUN_MODE_AP;
        }

    } else {
        ESP_LOGI(TAG, "No credentials → AP-only");
        esp_netif_create_default_wifi_ap();
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        configure_ap();
        ESP_ERROR_CHECK(esp_wifi_start());
        s_run_mode = WIFI_RUN_MODE_AP;
    }

    ESP_LOGI(TAG, "WiFi gotowy: %s",
             s_run_mode == WIFI_RUN_MODE_STA ? "STA" : "AP @ 192.168.4.1");
}

bool            wifi_is_connected(void)  { return s_connected; }
wifi_run_mode_t wifi_get_run_mode(void)  { return s_run_mode; }

const char *wifi_get_ap_ssid(void) { return WIFI_AP_SSID; }
const char *wifi_get_ap_pass(void) { return WIFI_AP_PASS; }
