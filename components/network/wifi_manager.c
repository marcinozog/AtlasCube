#include "defines.h"
#include "wifi_manager.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

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

// STA auto-connect is only wanted when we actually run as a client. The scan
// path (provisioning) brings up a STA interface purely to scan, so the STA_START
// / STA_DISCONNECTED handlers must not fire connect()/retry storms there.
static bool               s_sta_autoconnect = false;

// ── Scan state ───────────────────────────────────────────────────────────────
static bool                s_sta_netif_up   = false;   // STA netif created?
static volatile bool       s_scan_busy      = false;
static wifi_scan_ap_t      s_scan_results[WIFI_SCAN_MAX_AP];
static volatile int        s_scan_count     = 0;
static wifi_scan_done_cb_t s_scan_done_cb   = NULL;

// ─────────────────────────────────────────────────────────────────────────────
// OTA anti-brick: with CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE a freshly
// OTA-flashed app boots in PENDING_VERIFY and must confirm itself, or the
// bootloader rolls back to the previous slot on the next boot. We treat the
// first successful STA connection (got IP) as proof the new firmware works and
// mark it valid. One-shot and self-guarding: a no-op on normal boots (already
// VALID) and on the 8 MB factory layout (esp_ota_get_state_partition fails).
static void confirm_ota_image(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t   state;
    if (esp_ota_get_state_partition(running, &state) != ESP_OK) return;
    if (state != ESP_OTA_IMG_PENDING_VERIFY) return;

    if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
        ESP_LOGI(TAG, "OTA image confirmed valid — rollback cancelled");
    } else {
        ESP_LOGW(TAG, "OTA mark-valid failed");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
static void event_handler(void *arg, esp_event_base_t base,
                           int32_t id, void *data)
{
    if (base == WIFI_EVENT) {
        if (id == WIFI_EVENT_STA_START) {
            if (s_sta_autoconnect) esp_wifi_connect();

        } else if (id == WIFI_EVENT_SCAN_DONE) {
            uint16_t num = 0;
            esp_wifi_scan_get_ap_num(&num);
            wifi_ap_record_t *recs = NULL;
            if (num > 0) recs = calloc(num, sizeof(wifi_ap_record_t));
            if (recs && esp_wifi_scan_get_ap_records(&num, recs) == ESP_OK) {
                int n = 0;
                for (int i = 0; i < num && n < WIFI_SCAN_MAX_AP; i++) {
                    const char *ssid = (const char *)recs[i].ssid;
                    if (ssid[0] == '\0') continue;             // skip hidden
                    bool dup = false;                          // keep first (list is RSSI-sorted)
                    for (int j = 0; j < n; j++)
                        if (strcmp(s_scan_results[j].ssid, ssid) == 0) { dup = true; break; }
                    if (dup) continue;
                    strlcpy(s_scan_results[n].ssid, ssid, sizeof(s_scan_results[n].ssid));
                    s_scan_results[n].rssi   = recs[i].rssi;
                    s_scan_results[n].secure = recs[i].authmode != WIFI_AUTH_OPEN;
                    n++;
                }
                s_scan_count = n;
                ESP_LOGI(TAG, "Scan done: %d unique SSIDs (of %u)", n, num);
            } else {
                s_scan_count = 0;
                ESP_LOGW(TAG, "Scan done but no records");
            }
            free(recs);
            s_scan_busy = false;
            if (s_scan_done_cb) s_scan_done_cb();

        } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
            s_connected = false;
            if (!s_sta_autoconnect) {
                // Provisioning/scan STA interface — ignore disconnects entirely.
            } else if (s_retry_cnt < WIFI_STA_MAX_RETRY) {
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
        confirm_ota_image();   // network works → confirm a pending OTA image
    }
}

// ─────────────────────────────────────────────────────────────────────────────
static void configure_ap(void)
{
    wifi_config_t cfg = {0};
    memcpy(cfg.ap.ssid,     WIFI_AP_SSID, strlen(WIFI_AP_SSID));
    memcpy(cfg.ap.password, WIFI_AP_PASS, strlen(WIFI_AP_PASS));
    cfg.ap.ssid_len         = strlen(WIFI_AP_SSID);
    cfg.ap.channel          = WIFI_AP_CHANNEL;
    cfg.ap.max_connection   = WIFI_AP_MAX_CONN;
    cfg.ap.authmode         = WIFI_AUTH_WPA2_PSK;
    cfg.ap.pairwise_cipher  = WIFI_CIPHER_TYPE_CCMP;
    cfg.ap.pmf_cfg.capable  = true;
    cfg.ap.pmf_cfg.required = false;
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
    ESP_ERROR_CHECK(esp_wifi_set_country_code("PL", true));

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
        s_sta_netif_up    = true;
        s_sta_autoconnect = true;   // we want to connect as a client

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

// ─────────────────────────────────────────────────────────────────────────────
// WiFi scan
// ─────────────────────────────────────────────────────────────────────────────
void wifi_manager_set_scan_done_cb(wifi_scan_done_cb_t cb) { s_scan_done_cb = cb; }
bool wifi_manager_scan_busy(void)                          { return s_scan_busy; }

void wifi_manager_scan_start(void)
{
    if (s_scan_busy) { ESP_LOGW(TAG, "Scan already in progress"); return; }

    if (s_run_mode == WIFI_RUN_MODE_STA) {
        // Reconfiguration scan (Settings → WiFi while connected): STA can scan
        // without disconnecting — it just hops off-channel briefly (a streaming
        // radio may stall for a moment). Keep the connection and autoconnect
        // untouched; a network switch happens via save + restart, not here.
        ESP_LOGI(TAG, "Scan: STA mode — scanning without disconnecting");
    } else {
        // Provisioning path (AP mode): stop auto-reconnecting to any
        // previously-configured AP so the scan isn't fighting a connect/retry
        // loop (relevant when we fell back to AP after a failed STA attempt —
        // that STA netif already exists).
        s_sta_autoconnect = false;

        // Scanning needs a STA interface. In AP-only mode bring one up (the AP
        // stays online) and switch to APSTA.
        if (!s_sta_netif_up) {
            ESP_LOGI(TAG, "Scan: bringing up STA interface (AP stays up)");
            esp_netif_create_default_wifi_sta();
            s_sta_netif_up = true;
        }
        wifi_mode_t mode = WIFI_MODE_NULL;
        esp_wifi_get_mode(&mode);
        if (mode != WIFI_MODE_APSTA) {
            if (esp_wifi_set_mode(WIFI_MODE_APSTA) != ESP_OK)
                ESP_LOGE(TAG, "Scan: set APSTA failed");
        }
        esp_wifi_disconnect();   // cancel any stale/ongoing STA connect attempt
    }

    s_scan_count = 0;
    s_scan_busy  = true;
    esp_err_t err = esp_wifi_scan_start(NULL, false);   // async, all channels
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_scan_start failed: %s", esp_err_to_name(err));
        s_scan_busy = false;
        if (s_scan_done_cb) s_scan_done_cb();   // report completion (empty result)
    }
}

static int cmp_rssi_desc(const void *a, const void *b)
{
    return ((const wifi_scan_ap_t *)b)->rssi - ((const wifi_scan_ap_t *)a)->rssi;
}

int wifi_manager_scan_get(wifi_scan_ap_t *out, int max)
{
    if (!out || max <= 0) return 0;
    int n = s_scan_count;
    if (n > max) n = max;
    memcpy(out, s_scan_results, n * sizeof(wifi_scan_ap_t));
    qsort(out, n, sizeof(wifi_scan_ap_t), cmp_rssi_desc);
    return n;
}

const char *wifi_get_ip(char *buf, size_t len)
{
    const char *ifkey = (s_run_mode == WIFI_RUN_MODE_STA) ? "WIFI_STA_DEF"
                                                          : "WIFI_AP_DEF";
    esp_netif_t        *netif = esp_netif_get_handle_from_ifkey(ifkey);
    esp_netif_ip_info_t ip;
    if (netif && esp_netif_get_ip_info(netif, &ip) == ESP_OK && ip.ip.addr != 0) {
        snprintf(buf, len, IPSTR, IP2STR(&ip.ip));
    } else {
        snprintf(buf, len, "0.0.0.0");
    }
    return buf;
}
