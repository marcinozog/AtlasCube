#include "secrets.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "SECRETS";
#define SECRETS_NS "secrets"

esp_err_t secrets_set(const char *key, const char *value)
{
    nvs_handle_t h;
    // READWRITE creates the namespace on first use.
    esp_err_t err = nvs_open(SECRETS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;

    if (value && value[0] != '\0') {
        err = nvs_set_str(h, key, value);
    } else {
        // Empty / NULL → drop the key. Missing key is not an error.
        err = nvs_erase_key(h, key);
        if (err == ESP_ERR_NVS_NOT_FOUND) err = ESP_OK;
    }
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);

    if (err == ESP_OK) ESP_LOGI(TAG, "saved %s", key);
    else               ESP_LOGW(TAG, "save %s failed: %s", key, esp_err_to_name(err));
    return err;
}

bool secrets_get(const char *key, char *out, size_t out_sz)
{
    if (!out || out_sz == 0) return false;
    out[0] = '\0';

    nvs_handle_t h;
    if (nvs_open(SECRETS_NS, NVS_READONLY, &h) != ESP_OK) return false;

    size_t len = out_sz;
    esp_err_t err = nvs_get_str(h, key, out, &len);
    nvs_close(h);

    if (err != ESP_OK) {
        out[0] = '\0';
        return false;
    }
    return true;
}
