#include "board.h"
#include "board_def.h"

#include "esp_log.h"
#include "audio_mem.h"

#ifdef CONFIG_BOARD_ES3C28P
#include "driver/gpio.h"
#include "driver/i2c_master.h"
// Pulled in for the AUDIO_CODEC_ES8311_DEFAULT_HANDLE symbol below.
// We deliberately do NOT call es8311_codec_set_voice_volume() — it's
// static in some ADF builds and goes through a percentage formula that
// caps REG32 around +3.5 dB. We talk to the codec directly via I2C
// (see board_es8311_write_reg below) so we can pick any value 0x00..0xFF.
#include "es8311.h"

#define ES8311_I2C_ADDR     0x18    // 7-bit I2C slave address
#define ES8311_REG32_DAC    0x32    // DAC volume (0x00=mute, 0xBF=0dB, 0xFF=+32dB)
// DAC analog gain pinned at 0 dB. The audio_dsp volume scalar in
// audio_player.c then attenuates from there (linear taper, 0..100 → 0..1.0)
// which gives the UI slider a clean usable range:
//   vol=10  → -20 dB → quiet background
//   vol=25  → -12 dB → comfortable
//   vol=50  →  -6 dB → loud
//   vol=100 →   0 dB → maximum, just below clip
// Pushing this above 0 dB (e.g. 0xE5 = +13 dB as in the LCD-Wiki
// "music.ino" demo) made the UI slider unusable on this board — vol=4
// was already loud and vol=18 clipped. Keep at 0 dB unless someone
// wires up a quieter speaker.
#define ES8311_DAC_VOLUME   0xBF    // 0 dB — full 100-step UI slider range
#endif

#ifndef AUDIO_CODEC_DEFAULT_CONFIG
// Sample rate must match audio_player.c::PLAYBACK_SAMPLE_RATE (44100). In
// slave mode the ES8311 follows BCLK/LRCK, but its internal MCLK divider
// is configured from this `samples` field — a mismatch (e.g. 48K codec
// config vs 44.1K I2S) produces silence or heavy aliasing instead of music.
#define AUDIO_CODEC_DEFAULT_CONFIG() { \
    .adc_input  = AUDIO_HAL_ADC_INPUT_LINE1, \
    .dac_output = AUDIO_HAL_DAC_OUTPUT_ALL, \
    .codec_mode = AUDIO_HAL_CODEC_MODE_BOTH, \
    .i2s_iface = { \
        .mode = AUDIO_HAL_MODE_SLAVE, \
        .fmt = AUDIO_HAL_I2S_NORMAL, \
        .samples = AUDIO_HAL_44K_SAMPLES, \
        .bits = AUDIO_HAL_BIT_LENGTH_16BITS, \
    }, \
}
#endif

#ifndef AUDIO_CODEC_ES8311_DEFAULT_HANDLE
extern audio_hal_func_t AUDIO_CODEC_ES8311_DEFAULT_HANDLE;
#endif

static const char *TAG = "ATLAS_CUBE_BOARD";
static audio_board_handle_t board_handle = NULL;


#ifdef CONFIG_BOARD_ES3C28P
/* --------------------------------------------------------------------
   Direct ES8311 I2C write — works around the fact that ADF's
   audio_hal_set_volume() goes through a percentage-to-register formula
   that caps the DAC at REG32=0xC6 (+3.5 dB). On the ES3C28P board the
   speaker output is fundamentally quiet (SC8002B in a low-gain BTL
   config — confirmed via schematic), so we need every dB the codec can
   give us. Writing REG32 directly lets us set ANY value 0x00..0xFF.

   ADF created the I2C bus inside audio_hal_init via i2c_new_master_bus
   on I2C_NUM_0. We just grab its handle and add a new device entry
   for the codec address.
   -------------------------------------------------------------------- */
static esp_err_t board_es8311_write_reg(uint8_t reg, uint8_t val)
{
    i2c_master_bus_handle_t bus = NULL;
    esp_err_t err = i2c_master_get_bus_handle(I2C_NUM_0, &bus);
    if (err != ESP_OK || !bus) return err;

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = ES8311_I2C_ADDR,
        .scl_speed_hz    = 100000,
    };
    i2c_master_dev_handle_t dev = NULL;
    err = i2c_master_bus_add_device(bus, &dev_cfg, &dev);
    if (err != ESP_OK || !dev) return err;

    uint8_t buf[2] = { reg, val };
    err = i2c_master_transmit(dev, buf, sizeof(buf), 200);
    i2c_master_bus_rm_device(dev);
    return err;
}
#endif


audio_board_handle_t audio_board_init(void)
{
    if (board_handle) return board_handle;

    board_handle = audio_calloc(1, sizeof(struct audio_board_handle));
    if (!board_handle) return NULL;

    board_handle->audio_hal = audio_board_codec_init();

#ifdef CONFIG_BOARD_ES3C28P
    // SC8002B PA enable pin (GPIO1). Per ES32C28P spec and confirmed by
    // testing on hardware: LOW = amp enabled, HIGH = shutdown. We use
    // GPIO_DRIVE_CAP_3 (max strength) so the SD pin sees a clean rail
    // voltage even if a future revision lays out the trace through a
    // long via stack.
    gpio_config_t pa_cfg = {
        .pin_bit_mask = 1ULL << AUDIO_EN,
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&pa_cfg);
    gpio_set_drive_capability(AUDIO_EN, GPIO_DRIVE_CAP_3);
    gpio_set_level(AUDIO_EN, 0);
    ESP_LOGI(TAG, "SC8002B PA enabled (AUDIO_EN=LOW, GPIO1)");
#endif

    ESP_LOGI(TAG, "Custom board initialized");
    return board_handle;
}

audio_board_handle_t audio_board_get_handle(void)
{
    return board_handle;
}

esp_err_t audio_board_deinit(audio_board_handle_t audio_board)
{
    if (!audio_board) return ESP_FAIL;
    audio_free(audio_board);
    board_handle = NULL;
    return ESP_OK;
}

audio_hal_handle_t audio_board_codec_init(void)
{
#ifdef CONFIG_BOARD_ES3C28P
    // ES3C28P carries an ES8311 audio codec (I2C slave addr 0x18).
    // ADF v2.8 owns the I2C bus end-to-end: audio_hal_init() →
    // es8311_codec_init() → i2c_bus_create() → i2c_new_master_bus() on
    // I2C_NUM_0 with the pins from get_i2c_pins() in board_pins_config.c
    // (SDA=16/SCL=15). The touch controller shares this bus by grabbing
    // the handle via i2c_master_get_bus_handle() inside touch_init().
    audio_hal_codec_config_t audio_codec_cfg = AUDIO_CODEC_DEFAULT_CONFIG();
    audio_hal_handle_t codec_hal = audio_hal_init(&audio_codec_cfg,
                                                   &AUDIO_CODEC_ES8311_DEFAULT_HANDLE);
    if (!codec_hal) {
        ESP_LOGE(TAG, "audio_hal_init(ES8311) failed");
        return NULL;
    }
    audio_hal_ctrl_codec(codec_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);

    // Override DAC analog volume directly. ADF's audio_hal_set_volume
    // uses a different percentage formula and ends up around -6 dB at
    // its default which is unnecessarily quiet — we pin it at 0 dB so
    // the chain has plenty of room for the audio_dsp volume scalar to
    // attenuate cleanly down to "background" levels without ever
    // hitting the DAC's noise floor. See ES8311_DAC_VOLUME comment.
    board_es8311_write_reg(ES8311_REG32_DAC, ES8311_DAC_VOLUME);
    ESP_LOGI(TAG, "ES8311 codec ready (REG32=0x%02X / 0 dB)", ES8311_DAC_VOLUME);

    return codec_hal;
#else
    // AtlasCube uses PCM5102A passive DAC — no I2C codec, no HAL needed.
    return NULL;
#endif
}
