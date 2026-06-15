#include "mqtt_config.h"
#include "cJSON.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MQTT_CFG_FILE "/config/mqtt.json"

static const char *TAG = "MQTT_CFG";

static mqtt_config_t s_cfg;

static void copy_str(char *dst, size_t dst_sz, const char *src)
{
    dst[0] = '\0';
    if (src) strncpy(dst, src, dst_sz - 1);
}

const char *mqtt_widget_type_name(mqtt_widget_type_t t)
{
    switch (t) {
        case MQTT_W_TOGGLE: return "toggle";
        case MQTT_W_SLIDER: return "slider";
        case MQTT_W_LABEL:  return "label";
        default:            return "none";
    }
}

mqtt_widget_type_t mqtt_widget_type_from_name(const char *s)
{
    if (!s) return MQTT_W_NONE;
    if (strcmp(s, "toggle") == 0) return MQTT_W_TOGGLE;
    if (strcmp(s, "slider") == 0) return MQTT_W_SLIDER;
    if (strcmp(s, "label")  == 0) return MQTT_W_LABEL;
    return MQTT_W_NONE;
}

mqtt_config_t *mqtt_config_get(void) { return &s_cfg; }

static void apply_defaults(void)
{
    memset(&s_cfg, 0, sizeof(s_cfg));
    s_cfg.enabled = false;
    s_cfg.port = 1883;
    copy_str(s_cfg.client_id,  sizeof(s_cfg.client_id),  "atlascube");
    copy_str(s_cfg.base_topic, sizeof(s_cfg.base_topic), "atlascube");
    for (int i = 0; i < MQTT_MAX_WIDGETS; ++i) {
        s_cfg.widgets[i].type = MQTT_W_NONE;
        s_cfg.widgets[i].min  = 0;
        s_cfg.widgets[i].max  = 100;
        s_cfg.widgets[i].step = 1;
    }
    s_cfg.screensaver.title[0]       = '\0';
    s_cfg.screensaver.topic_state[0] = '\0';
    s_cfg.screensaver.json_path[0]   = '\0';
    s_cfg.screensaver.unit[0]        = '\0';
}

static void parse_widget(cJSON *w, mqtt_widget_t *out)
{
    out->type = MQTT_W_NONE;
    out->title[0]       = '\0';
    out->topic_cmd[0]   = '\0';
    out->topic_state[0] = '\0';
    out->json_path[0]   = '\0';
    out->unit[0]        = '\0';
    out->min = 0; out->max = 100; out->step = 1;

    if (!cJSON_IsObject(w)) return;

    cJSON *t = cJSON_GetObjectItem(w, "type");
    if (cJSON_IsString(t)) out->type = mqtt_widget_type_from_name(t->valuestring);

    cJSON *j;
    j = cJSON_GetObjectItem(w, "title");       if (cJSON_IsString(j)) copy_str(out->title,       sizeof(out->title),       j->valuestring);
    j = cJSON_GetObjectItem(w, "topic_cmd");   if (cJSON_IsString(j)) copy_str(out->topic_cmd,   sizeof(out->topic_cmd),   j->valuestring);
    j = cJSON_GetObjectItem(w, "topic_state"); if (cJSON_IsString(j)) copy_str(out->topic_state, sizeof(out->topic_state), j->valuestring);
    j = cJSON_GetObjectItem(w, "json_path");   if (cJSON_IsString(j)) copy_str(out->json_path,   sizeof(out->json_path),   j->valuestring);
    j = cJSON_GetObjectItem(w, "unit");        if (cJSON_IsString(j)) copy_str(out->unit,        sizeof(out->unit),        j->valuestring);
    j = cJSON_GetObjectItem(w, "min");         if (cJSON_IsNumber(j)) out->min  = j->valueint;
    j = cJSON_GetObjectItem(w, "max");         if (cJSON_IsNumber(j)) out->max  = j->valueint;
    j = cJSON_GetObjectItem(w, "step");        if (cJSON_IsNumber(j)) out->step = j->valueint;
    if (out->step < 1) out->step = 1;
}

esp_err_t mqtt_config_load(void)
{
    apply_defaults();

    FILE *f = fopen(MQTT_CFG_FILE, "r");
    if (!f) {
        ESP_LOGI(TAG, "no %s — using defaults", MQTT_CFG_FILE);
        return ESP_OK;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    if (sz <= 0 || sz > 16384) { fclose(f); return ESP_FAIL; }

    char *buf = malloc(sz + 1);
    if (!buf) { fclose(f); return ESP_ERR_NO_MEM; }
    fread(buf, 1, sz, f);
    buf[sz] = '\0';
    fclose(f);

    cJSON *json = cJSON_Parse(buf);
    free(buf);
    if (!json) {
        ESP_LOGW(TAG, "%s parse failed", MQTT_CFG_FILE);
        return ESP_FAIL;
    }

    cJSON *j;
    j = cJSON_GetObjectItem(json, "enabled");    if (cJSON_IsBool(j))   s_cfg.enabled = cJSON_IsTrue(j);
    j = cJSON_GetObjectItem(json, "host");       if (cJSON_IsString(j)) copy_str(s_cfg.host,       sizeof(s_cfg.host),       j->valuestring);
    j = cJSON_GetObjectItem(json, "port");       if (cJSON_IsNumber(j)) s_cfg.port = j->valueint;
    j = cJSON_GetObjectItem(json, "username");   if (cJSON_IsString(j)) copy_str(s_cfg.username,   sizeof(s_cfg.username),   j->valuestring);
    j = cJSON_GetObjectItem(json, "password");   if (cJSON_IsString(j)) copy_str(s_cfg.password,   sizeof(s_cfg.password),   j->valuestring);
    j = cJSON_GetObjectItem(json, "client_id");  if (cJSON_IsString(j)) copy_str(s_cfg.client_id,  sizeof(s_cfg.client_id),  j->valuestring);
    j = cJSON_GetObjectItem(json, "base_topic"); if (cJSON_IsString(j)) copy_str(s_cfg.base_topic, sizeof(s_cfg.base_topic), j->valuestring);

    cJSON *arr = cJSON_GetObjectItem(json, "widgets");
    if (cJSON_IsArray(arr)) {
        int n = cJSON_GetArraySize(arr);
        if (n > MQTT_MAX_WIDGETS) n = MQTT_MAX_WIDGETS;
        for (int i = 0; i < n; ++i) {
            parse_widget(cJSON_GetArrayItem(arr, i), &s_cfg.widgets[i]);
        }
    }

    cJSON *ss = cJSON_GetObjectItem(json, "screensaver");
    if (cJSON_IsObject(ss)) {
        j = cJSON_GetObjectItem(ss, "title");       if (cJSON_IsString(j)) copy_str(s_cfg.screensaver.title,       sizeof(s_cfg.screensaver.title),       j->valuestring);
        j = cJSON_GetObjectItem(ss, "topic_state"); if (cJSON_IsString(j)) copy_str(s_cfg.screensaver.topic_state, sizeof(s_cfg.screensaver.topic_state), j->valuestring);
        j = cJSON_GetObjectItem(ss, "json_path");   if (cJSON_IsString(j)) copy_str(s_cfg.screensaver.json_path,   sizeof(s_cfg.screensaver.json_path),   j->valuestring);
        j = cJSON_GetObjectItem(ss, "unit");        if (cJSON_IsString(j)) copy_str(s_cfg.screensaver.unit,        sizeof(s_cfg.screensaver.unit),        j->valuestring);
    }

    cJSON_Delete(json);
    return ESP_OK;
}

esp_err_t mqtt_config_save(void)
{
    cJSON *json = cJSON_CreateObject();
    cJSON_AddBoolToObject  (json, "enabled",    s_cfg.enabled);
    cJSON_AddStringToObject(json, "host",       s_cfg.host);
    cJSON_AddNumberToObject(json, "port",       s_cfg.port);
    cJSON_AddStringToObject(json, "username",   s_cfg.username);
    cJSON_AddStringToObject(json, "password",   s_cfg.password);
    cJSON_AddStringToObject(json, "client_id",  s_cfg.client_id);
    cJSON_AddStringToObject(json, "base_topic", s_cfg.base_topic);

    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < MQTT_MAX_WIDGETS; ++i) {
        mqtt_widget_t *w = &s_cfg.widgets[i];
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "type",        mqtt_widget_type_name(w->type));
        cJSON_AddStringToObject(o, "title",       w->title);
        cJSON_AddStringToObject(o, "topic_cmd",   w->topic_cmd);
        cJSON_AddStringToObject(o, "topic_state", w->topic_state);
        cJSON_AddStringToObject(o, "json_path",   w->json_path);
        cJSON_AddStringToObject(o, "unit",        w->unit);
        cJSON_AddNumberToObject(o, "min",         w->min);
        cJSON_AddNumberToObject(o, "max",         w->max);
        cJSON_AddNumberToObject(o, "step",        w->step);
        cJSON_AddItemToArray(arr, o);
    }
    cJSON_AddItemToObject(json, "widgets", arr);

    cJSON *ss = cJSON_CreateObject();
    cJSON_AddStringToObject(ss, "title",       s_cfg.screensaver.title);
    cJSON_AddStringToObject(ss, "topic_state", s_cfg.screensaver.topic_state);
    cJSON_AddStringToObject(ss, "json_path",   s_cfg.screensaver.json_path);
    cJSON_AddStringToObject(ss, "unit",        s_cfg.screensaver.unit);
    cJSON_AddItemToObject(json, "screensaver", ss);

    char *str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    if (!str) return ESP_ERR_NO_MEM;

    // atomic write via tmp file (SPIFFS does not allow rename onto existing)
    const char *tmp = MQTT_CFG_FILE ".tmp";
    FILE *f = fopen(tmp, "w");
    if (!f) { free(str); return ESP_FAIL; }
    size_t len = strlen(str);
    size_t written = fwrite(str, 1, len, f);
    fclose(f);
    free(str);
    if (written != len) { remove(tmp); return ESP_FAIL; }

    remove(MQTT_CFG_FILE);
    if (rename(tmp, MQTT_CFG_FILE) != 0) {
        ESP_LOGE(TAG, "rename %s → %s failed", tmp, MQTT_CFG_FILE);
        return ESP_FAIL;
    }
    return ESP_OK;
}
