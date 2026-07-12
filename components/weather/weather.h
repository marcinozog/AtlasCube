#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    WEATHER_PROVIDER_OPEN_METEO     = 0,   // no API key needed
    WEATHER_PROVIDER_OPENWEATHERMAP = 1,   // needs api_key
} weather_provider_t;

typedef struct {
    bool  enabled;
    int   provider;          // weather_provider_t
    char  api_key[64];       // OpenWeatherMap key — plain config, exports with it
    float latitude;
    float longitude;
    int   refresh_min;       // clamped to 5..240
} weather_config_t;

typedef struct {
    bool  valid;
    float temperature_c;
    float apparent_temperature_c;
    int   humidity_pct;
    int   weather_code;      // WMO weather interpretation code
    bool  is_day;
    int64_t updated_us;      // monotonic time of the last successful fetch
} weather_data_t;

typedef void (*weather_update_cb_t)(void);

void weather_init(void);
void weather_get_config(weather_config_t *out);
esp_err_t weather_set_config(const weather_config_t *config);
void weather_get(weather_data_t *out);
void weather_set_update_cb(weather_update_cb_t cb);

