#include "weather.h"

#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WEATHER_CONFIG_FILE "/config/weather.json"
#define WEATHER_BODY_CAP    1536
#define WEATHER_STACK       5120

static const char *TAG = "WEATHER";
static SemaphoreHandle_t s_lock;
static weather_config_t s_config = { .enabled = false,
                                     .provider = WEATHER_PROVIDER_OPEN_METEO,
                                     .latitude = 52.2297f,
                                     .longitude = 21.0122f, .refresh_min = 30 };
static weather_data_t s_data;
static weather_update_cb_t s_update_cb;
static TaskHandle_t s_task;
static unsigned s_generation;

static void lock(void)   { xSemaphoreTake(s_lock, portMAX_DELAY); }
static void unlock(void) { xSemaphoreGive(s_lock); }

static void config_normalize(weather_config_t *c)
{
    if (c->provider != WEATHER_PROVIDER_OPENWEATHERMAP)
        c->provider = WEATHER_PROVIDER_OPEN_METEO;
    // Keys are plain alphanumeric — dropping anything else keeps the
    // fprintf-built JSON and the request URL safe.
    char *w = c->api_key;
    for (const char *r = c->api_key; *r; ++r)
        if (isalnum((unsigned char)*r)) *w++ = *r;
    *w = '\0';
    if (c->latitude < -90.0f) c->latitude = -90.0f;
    if (c->latitude > 90.0f) c->latitude = 90.0f;
    if (c->longitude < -180.0f) c->longitude = -180.0f;
    if (c->longitude > 180.0f) c->longitude = 180.0f;
    if (c->refresh_min < 5) c->refresh_min = 5;
    if (c->refresh_min > 240) c->refresh_min = 240;
}

static void config_load(void)
{
    FILE *f = fopen(WEATHER_CONFIG_FILE, "r");
    if (!f) return;
    char buf[384];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = '\0';
    cJSON *root = cJSON_Parse(buf);
    if (!root) return;
    cJSON *v = cJSON_GetObjectItem(root, "enabled");
    if (cJSON_IsBool(v)) s_config.enabled = cJSON_IsTrue(v);
    v = cJSON_GetObjectItem(root, "provider");
    if (cJSON_IsNumber(v)) s_config.provider = v->valueint;
    v = cJSON_GetObjectItem(root, "api_key");
    if (cJSON_IsString(v)) {
        strncpy(s_config.api_key, v->valuestring, sizeof(s_config.api_key) - 1);
        s_config.api_key[sizeof(s_config.api_key) - 1] = '\0';
    }
    v = cJSON_GetObjectItem(root, "latitude");
    if (cJSON_IsNumber(v)) s_config.latitude = (float)v->valuedouble;
    v = cJSON_GetObjectItem(root, "longitude");
    if (cJSON_IsNumber(v)) s_config.longitude = (float)v->valuedouble;
    v = cJSON_GetObjectItem(root, "refresh_min");
    if (cJSON_IsNumber(v)) s_config.refresh_min = v->valueint;
    config_normalize(&s_config);
    cJSON_Delete(root);
}

static esp_err_t config_save(const weather_config_t *c)
{
    FILE *f = fopen(WEATHER_CONFIG_FILE, "w");
    if (!f) return ESP_FAIL;
    int n = fprintf(f, "{\"enabled\":%s,\"provider\":%d,\"api_key\":\"%s\",\"latitude\":%.5f,\"longitude\":%.5f,\"refresh_min\":%d}\n",
                    c->enabled ? "true" : "false", c->provider, c->api_key,
                    c->latitude, c->longitude, c->refresh_min);
    fclose(f);
    return n > 0 ? ESP_OK : ESP_FAIL;
}

static esp_err_t http_get(const char *url, char *body, size_t cap)
{
    esp_http_client_config_t hc = {
        .url = url, .timeout_ms = 12000, .crt_bundle_attach = esp_crt_bundle_attach,
        .keep_alive_enable = true,
    };
    esp_http_client_handle_t cli = esp_http_client_init(&hc);
    if (!cli) return ESP_ERR_NO_MEM;
    esp_err_t err = esp_http_client_open(cli, 0);
    if (err != ESP_OK) { esp_http_client_cleanup(cli); return err; }
    // Headers must be parsed before reading the body — without this the
    // status code stays 0 and every fetch is treated as failed.
    if (esp_http_client_fetch_headers(cli) < 0) {
        esp_http_client_close(cli);
        esp_http_client_cleanup(cli);
        return ESP_FAIL;
    }
    int total = 0;
    while (total < (int)cap - 1) {
        int got = esp_http_client_read(cli, body + total, cap - 1 - total);
        if (got <= 0) break;
        total += got;
    }
    body[total] = '\0';
    int status = esp_http_client_get_status_code(cli);
    esp_http_client_close(cli);
    esp_http_client_cleanup(cli);
    return status == 200 && total > 0 ? ESP_OK : ESP_FAIL;
}

static bool num(cJSON *root, const char *name, double *out)
{
    cJSON *v = cJSON_GetObjectItem(root, name);
    if (!cJSON_IsNumber(v)) return false;
    *out = v->valuedouble;
    return true;
}

static bool parse_open_meteo(cJSON *root, weather_data_t *out)
{
    cJSON *current = cJSON_GetObjectItem(root, "current");
    double temp, apparent, humidity, code, is_day;
    bool ok = cJSON_IsObject(current)
           && num(current, "temperature_2m", &temp)
           && num(current, "apparent_temperature", &apparent)
           && num(current, "relative_humidity_2m", &humidity)
           && num(current, "weather_code", &code)
           && num(current, "is_day", &is_day);
    if (ok) {
        *out = (weather_data_t){ .valid = true, .temperature_c = (float)temp,
            .apparent_temperature_c = (float)apparent, .humidity_pct = (int)humidity,
            .weather_code = (int)code, .is_day = is_day != 0,
            .updated_us = esp_timer_get_time() };
    }
    return ok;
}

// OpenWeatherMap condition ids → the WMO codes the rest of the stack speaks
// (weather_widget.c condition() and weatherCondition() in settings.js).
static int owm_to_wmo(int id)
{
    if (id >= 200 && id < 300) return 95;   // thunderstorm
    if (id >= 300 && id < 400) return 51;   // drizzle
    if (id == 511)             return 66;   // freezing rain
    if (id >= 520 && id < 600) return 80;   // shower rain
    if (id >= 500 && id < 520) return 61;   // rain
    if (id >= 600 && id < 700) return 71;   // snow
    if (id >= 700 && id < 800) return 45;   // mist / fog / atmosphere
    if (id == 800)             return 0;    // clear
    if (id == 801 || id == 802) return 1;   // few / scattered clouds
    return 3;                               // 803 / 804 — overcast
}

static bool parse_owm(cJSON *root, weather_data_t *out)
{
    cJSON *main_o = cJSON_GetObjectItem(root, "main");
    cJSON *w_arr  = cJSON_GetObjectItem(root, "weather");
    cJSON *w0     = cJSON_IsArray(w_arr) ? cJSON_GetArrayItem(w_arr, 0) : NULL;
    cJSON *sys_o  = cJSON_GetObjectItem(root, "sys");
    double temp, feels, humidity, id, dt, sunrise, sunset;
    bool ok = cJSON_IsObject(main_o) && cJSON_IsObject(w0)
           && num(main_o, "temp", &temp)
           && num(main_o, "feels_like", &feels)
           && num(main_o, "humidity", &humidity)
           && num(w0, "id", &id)
           && num(root, "dt", &dt);
    if (!ok) return false;
    bool day = true;
    if (cJSON_IsObject(sys_o) && num(sys_o, "sunrise", &sunrise)
                              && num(sys_o, "sunset", &sunset))
        day = dt >= sunrise && dt < sunset;
    *out = (weather_data_t){ .valid = true, .temperature_c = (float)temp,
        .apparent_temperature_c = (float)feels, .humidity_pct = (int)humidity,
        .weather_code = owm_to_wmo((int)id), .is_day = day,
        .updated_us = esp_timer_get_time() };
    return true;
}

static bool fetch(weather_data_t *out, const weather_config_t *cfg)
{
    char url[256];
    bool owm = cfg->provider == WEATHER_PROVIDER_OPENWEATHERMAP;
    if (owm) {
        if (!cfg->api_key[0]) {
            ESP_LOGW(TAG, "OpenWeatherMap API key not set");
            return false;
        }
        snprintf(url, sizeof(url),
                 "https://api.openweathermap.org/data/2.5/weather?lat=%.5f&lon=%.5f&units=metric&appid=%s",
                 cfg->latitude, cfg->longitude, cfg->api_key);
    } else {
        snprintf(url, sizeof(url),
                 "https://api.open-meteo.com/v1/forecast?latitude=%.5f&longitude=%.5f&current=temperature_2m,apparent_temperature,relative_humidity_2m,weather_code,is_day",
                 cfg->latitude, cfg->longitude);
    }
    char *body = malloc(WEATHER_BODY_CAP);
    if (!body) return false;
    esp_err_t err = http_get(url, body, WEATHER_BODY_CAP);
    if (err != ESP_OK) { free(body); return false; }
    cJSON *root = cJSON_Parse(body);
    free(body);
    if (!root) return false;
    bool ok = owm ? parse_owm(root, out) : parse_open_meteo(root, out);
    cJSON_Delete(root);
    return ok;
}

static void weather_task(void *arg)
{
    (void)arg;
    unsigned seen = 0;
    for (;;) {
        weather_config_t cfg;
        bool valid;
        lock(); cfg = s_config; unsigned generation = s_generation; valid = s_data.valid; unlock();
        if (cfg.enabled && (generation != seen || !valid)) {
            weather_data_t fresh;
            if (fetch(&fresh, &cfg)) {
                weather_update_cb_t cb;
                lock(); s_data = fresh; cb = s_update_cb; unlock();
                if (cb) cb();
            } else {
                ESP_LOGW(TAG, "fetch failed");
            }
            seen = generation;
        }
        // Re-read configuration frequently enough for a web update to apply;
        // the timestamp controls normal polling without a busy wait.
        uint32_t wait_s = cfg.enabled ? (uint32_t)cfg.refresh_min * 60U : 5U;
        for (uint32_t i = 0; i < wait_s; ++i) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            lock(); bool changed = s_generation != seen; unlock();
            if (changed) break;
        }
        lock(); valid = s_data.valid; unlock();
        if (cfg.enabled && valid) seen = 0; // fetch again after interval
    }
}

void weather_init(void)
{
    if (s_lock) return;
    s_lock = xSemaphoreCreateMutex();
    config_load();
    xTaskCreate(weather_task, "weather", WEATHER_STACK, NULL, 4, &s_task);
}

void weather_get_config(weather_config_t *out)
{
    if (!out) return;
    if (!s_lock) { *out = s_config; return; }
    lock(); *out = s_config; unlock();
}

esp_err_t weather_set_config(const weather_config_t *config)
{
    if (!config) return ESP_ERR_INVALID_ARG;
    weather_config_t c = *config;
    config_normalize(&c);
    esp_err_t err = config_save(&c);
    if (err != ESP_OK) return err;
    if (!s_lock) { s_config = c; s_data.valid = false; ++s_generation; return ESP_OK; }
    lock(); s_config = c; s_data.valid = false; ++s_generation; unlock();
    return ESP_OK;
}

void weather_get(weather_data_t *out)
{
    if (!out) return;
    if (!s_lock) { memset(out, 0, sizeof(*out)); return; }
    lock(); *out = s_data; unlock();
}

void weather_set_update_cb(weather_update_cb_t cb)
{
    if (!s_lock) { s_update_cb = cb; return; }
    lock(); s_update_cb = cb; unlock();
}
