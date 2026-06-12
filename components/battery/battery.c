#include "battery.h"
#include "defines.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"

static const char *TAG = "BATTERY";

// 12 dB attenuation = ~3.1 V full-scale on ESP32-S3, so with the 1:1 divider
// we can read battery voltages up to ~6.2 V — well above the 4.2 V Li-Po max.
#define BAT_ADC_ATTEN          ADC_ATTEN_DB_12
#define BAT_ADC_BITWIDTH       ADC_BITWIDTH_DEFAULT
#define BAT_SAMPLE_BURST       16        // averaged per read
#define BAT_DIVIDER_RATIO      2         // hardware divider is 1:1 → ×2 compensation

static adc_oneshot_unit_handle_t s_adc_unit = NULL;
static adc_cali_handle_t         s_adc_cali = NULL;
static adc_channel_t             s_adc_chan = ADC_CHANNEL_0;
static bool                      s_inited   = false;

esp_err_t battery_init(void)
{
    if (s_inited) return ESP_OK;

    if (BAT_ADC_PIN < 0) {
        ESP_LOGI(TAG, "Battery monitor disabled on this board (BAT_ADC_PIN=%d)",
                 BAT_ADC_PIN);
        return ESP_ERR_NOT_SUPPORTED;
    }

    // Resolve GPIO → ADC1 channel. ESP32-S3: GPIO1..GPIO10 → ADC1_CH0..CH9.
    adc_unit_t unit;
    esp_err_t err = adc_oneshot_io_to_channel(BAT_ADC_PIN, &unit, &s_adc_chan);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "GPIO%d is not an ADC pin: %s",
                 (int)BAT_ADC_PIN, esp_err_to_name(err));
        return err;
    }
    if (unit != ADC_UNIT_1) {
        ESP_LOGE(TAG, "GPIO%d is on ADC%d, expected ADC1", (int)BAT_ADC_PIN,
                 (int)unit + 1);
        return ESP_ERR_INVALID_ARG;
    }

    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id  = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    err = adc_oneshot_new_unit(&unit_cfg, &s_adc_unit);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "adc_oneshot_new_unit failed: %s", esp_err_to_name(err));
        return err;
    }

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten    = BAT_ADC_ATTEN,
        .bitwidth = BAT_ADC_BITWIDTH,
    };
    err = adc_oneshot_config_channel(s_adc_unit, s_adc_chan, &chan_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "adc_oneshot_config_channel failed: %s", esp_err_to_name(err));
        adc_oneshot_del_unit(s_adc_unit);
        s_adc_unit = NULL;
        return err;
    }

    // Curve-fitting calibration is the recommended scheme on ESP32-S3.
    // If eFuse calibration data is missing (very old chips) we log a warning
    // and fall back to uncalibrated raw → mV conversion in the read path.
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id  = ADC_UNIT_1,
        .chan     = s_adc_chan,
        .atten    = BAT_ADC_ATTEN,
        .bitwidth = BAT_ADC_BITWIDTH,
    };
    err = adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_adc_cali);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Calibration unavailable (%s) — readings will be approximate",
                 esp_err_to_name(err));
        s_adc_cali = NULL;
    }

    s_inited = true;
    ESP_LOGI(TAG, "Initialized on GPIO%d (ADC1_CH%d, atten=12dB, divider=1:%d)",
             (int)BAT_ADC_PIN, (int)s_adc_chan, BAT_DIVIDER_RATIO);
    return ESP_OK;
}

int battery_read_mv(void)
{
    if (!s_inited) return -1;

    int sum   = 0;
    int valid = 0;
    for (int i = 0; i < BAT_SAMPLE_BURST; ++i) {
        int raw = 0;
        if (adc_oneshot_read(s_adc_unit, s_adc_chan, &raw) != ESP_OK) continue;

        int mv = 0;
        if (s_adc_cali) {
            if (adc_cali_raw_to_voltage(s_adc_cali, raw, &mv) != ESP_OK) continue;
        } else {
            // Coarse fallback: 12-bit raw spans 0..3100 mV at 12 dB atten.
            // Off by a few % vs calibrated reading, but better than nothing.
            mv = (raw * 3100) / 4095;
        }
        sum += mv;
        valid++;
    }
    if (valid == 0) return -1;

    int avg_mv  = sum / valid;
    int vbat_mv = avg_mv * BAT_DIVIDER_RATIO;
    return vbat_mv;
}

int battery_read_percent(void)
{
    int mv = battery_read_mv();
    if (mv < 0) return -1;

    // Piecewise-linear Li-Po resting-voltage SoC curve.
    // Points are listed from full to empty; values are { mV, percent }.
    static const int curve[][2] = {
        { 4200, 100 },
        { 4100,  90 },
        { 4000,  80 },
        { 3900,  60 },
        { 3800,  45 },
        { 3700,  30 },
        { 3600,  18 },
        { 3500,  10 },
        { 3400,   5 },
        { 3300,   0 },
    };
    const int N = (int)(sizeof(curve) / sizeof(curve[0]));

    if (mv >= curve[0][0])      return 100;
    if (mv <= curve[N - 1][0])  return 0;

    for (int i = 0; i < N - 1; ++i) {
        int v_hi = curve[i][0],     p_hi = curve[i][1];
        int v_lo = curve[i + 1][0], p_lo = curve[i + 1][1];
        if (mv <= v_hi && mv >= v_lo) {
            int span_v = v_hi - v_lo;
            int span_p = p_hi - p_lo;
            return p_lo + ((mv - v_lo) * span_p) / span_v;
        }
    }
    return -1;
}

bool battery_is_available(void)
{
    return s_inited;
}
