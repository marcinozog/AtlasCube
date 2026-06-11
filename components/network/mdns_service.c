#include "mdns_service.h"
#include "settings.h"
#include "mdns.h"
#include "esp_mac.h"
#include "esp_log.h"
#include <string.h>
#include <ctype.h>

static const char *TAG = "MDNS";

static bool s_started = false;

// Build the effective hostname into buf:
//   - non-empty settings value → sanitized to a valid DNS label
//   - empty → "atlascube-xxxx" from the last two MAC bytes
static void resolve_hostname(char *buf, size_t len)
{
    const char *cfg = settings_get()->device.hostname;

    if (cfg && cfg[0] != '\0') {
        // Sanitize: lowercase, keep [a-z0-9-], collapse anything else to '-'.
        size_t j = 0;
        for (size_t i = 0; cfg[i] && j < len - 1; i++) {
            char c = (char)tolower((unsigned char)cfg[i]);
            buf[j++] = (isalnum((unsigned char)c) || c == '-') ? c : '-';
        }
        buf[j] = '\0';
        if (j > 0) return;   // fall through to auto name only if it sanitized to empty
    }

    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(buf, len, "atlascube-%02x%02x", mac[4], mac[5]);
}

// Publish/refresh the "_http._tcp" TXT record carrying the full ".local" name
// so discovery clients can show/use the hostname without reading the SRV target.
static void publish_txt(const char *host)
{
    char fqdn[40];
    snprintf(fqdn, sizeof(fqdn), "%s.local", host);
    mdns_service_txt_item_set("_http", "_tcp", "host", fqdn);
    mdns_service_txt_item_set("_http", "_tcp", "path", "/");
}

void mdns_service_start(void)
{
    if (s_started) return;

    esp_err_t err = mdns_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mdns_init failed: %s", esp_err_to_name(err));
        return;
    }

    char host[32];
    resolve_hostname(host, sizeof(host));

    // Materialize an empty (auto) hostname into settings on first start so the
    // web UI shows the actual name instead of a bare placeholder, and the MAC
    // name stays stable as the saved value the user can later edit.
    if (settings_get()->device.hostname[0] == '\0') {
        settings_set_hostname(host);
    }

    mdns_hostname_set(host);
    mdns_instance_name_set("AtlasCube Radio");
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
    publish_txt(host);   // expose "<host>.local" for clients (Android NsdManager
                         // hides the SRV target after resolve)

    s_started = true;
    ESP_LOGI(TAG, "mDNS up: http://%s.local", host);
}

void mdns_service_apply_hostname(void)
{
    if (!s_started) return;

    char host[32];
    resolve_hostname(host, sizeof(host));

    // A cleared field reverts to the auto MAC name — persist it so the saved
    // value matches what mDNS actually advertises (same as on first start).
    if (settings_get()->device.hostname[0] == '\0') {
        settings_set_hostname(host);
    }

    mdns_hostname_set(host);
    publish_txt(host);   // keep the TXT "host" in sync with the new name
    ESP_LOGI(TAG, "mDNS hostname → http://%s.local", host);
}

const char *mdns_effective_hostname(char *buf, size_t len)
{
    resolve_hostname(buf, len);
    return buf;
}
