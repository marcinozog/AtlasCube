#include "audio_player.h"
#include "defines.h"
#include "audio_pipeline.h"
#include "audio_element.h"
#include "audio_dsp.h"
#include "i2s_stream.h"
#include "icy_http_stream.h"
#include "mp3_decoder.h"
#include "aac_decoder.h"
#include "flac_decoder.h"
#include "filter_resample.h"
#include "esp_log.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "settings.h"
#include "app_state.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_heap_caps.h"
#include <math.h>
#include <strings.h>

static const char *TAG = "AUDIO_PLAYER";

static void audio_event_task(void *param);
static void retry_task(void *param);
static void audio_play_task(void *param);
static void audio_play_internal(const char *url);
static int http_event_handler(icy_http_event_msg_t *msg);
static void parse_stream_title(const char *meta);

#define PLAY_URL_MAX 512

typedef enum {
    CODEC_UNKNOWN = -1,
    CODEC_MP3 = 0,
    CODEC_AAC,
    CODEC_FLAC,
} codec_type_t;

static const char *codec_name[] = { "mp3", "aac", "flac" };

#define PLAYBACK_SAMPLE_RATE 44100
#define PLAYBACK_CHANNELS    2
#define PLAYBACK_BITS        16

static audio_pipeline_handle_t pipeline;
static audio_element_handle_t
    http_stream_reader, mp3_decoder_el, aac_decoder_el, flac_decoder_el,
    active_decoder_el, rsp_filter_el, dsp_el, i2s_stream_writer;

static audio_event_iface_handle_t evt;

static bool is_playing = false;
static codec_type_t current_codec = CODEC_MP3;

static TaskHandle_t s_retry_task_handle = NULL;
static int s_retry_count = 0;
#define MAX_RETRIES 5

// All heavy work (HTTP probe + TLS handshake + pipeline build) goes to a
// dedicated audio_play_task with an 8 KB stack. Previously audio_player_play
// was called directly from the httpd/WS task (~4 KB stack) and the TLS
// handshake caused stack overflow.
static SemaphoreHandle_t s_play_sem  = NULL;  // binary: trigger
static SemaphoreHandle_t s_play_lock = NULL;  // mutex: protects s_pending_url
static char              s_pending_url[PLAY_URL_MAX];

static int last_rsp_sample_rate = 0;
static int last_rsp_channels    = 0;


/*
static codec_type_t detect_codec_from_url(const char *url)
Quick URL-based detection — no HTTP request. Returns CODEC_UNKNOWN when the
URL has no readable hint (typical SHOUTcast/Icecast endpoints without an
extension, e.g. /1, /stream). In that case we call detect_codec_from_http.
*/
static codec_type_t detect_codec_from_url(const char *url)
{
    if (!url) return CODEC_UNKNOWN;

    // .flac must include the dot — bare "flac" in a stream name may mean
    // OGG/FLAC (audio/ogg, e.g. juventus_FLAC), which is not supported,
    // so without the dot we go to probe instead of assuming CODEC_FLAC.
    if (strcasestr(url, ".flac")) {
        return CODEC_FLAC;
    }

    if (strcasestr(url, ".aac")  ||
        strcasestr(url, ".aacp") ||
        strcasestr(url, ".adts") ||
        strcasestr(url, "aacp")  ||   // e.g. /stream.aacp or /live_aacp
        strcasestr(url, "/aac")) {    // e.g. /stream/aac/128
        return CODEC_AAC;
    }

    if (strcasestr(url, ".mp3")) {
        return CODEC_MP3;
    }

    return CODEC_UNKNOWN;
}


/*
static esp_err_t probe_event_handler(esp_http_client_event_t *evt)
Captures Content-Type from HTTP_EVENT_ON_HEADER. esp_http_client_get_header
does not always return this value (depends on backend/configuration), so
the event handler is more reliable.
*/
typedef struct {
    char content_type[64];
} probe_ctx_t;

static esp_err_t probe_event_handler(esp_http_client_event_t *evt)
{
    if (evt->event_id == HTTP_EVENT_ON_HEADER && evt->user_data
        && evt->header_key && evt->header_value
        && strcasecmp(evt->header_key, "Content-Type") == 0) {

        probe_ctx_t *ctx = (probe_ctx_t *)evt->user_data;
        strncpy(ctx->content_type, evt->header_value,
                sizeof(ctx->content_type) - 1);
        ctx->content_type[sizeof(ctx->content_type) - 1] = '\0';
    }
    return ESP_OK;
}


/*
static codec_type_t detect_codec_from_http(const char *url)
Pre-flight HTTP request — fetches the stream headers (Range: bytes=0-1 to
avoid downloading the whole thing) and maps Content-Type to a codec. Used
when the URL has no readable extension. ~100-300ms latency. Falls back to
CODEC_UNKNOWN.
*/
static codec_type_t detect_codec_from_http(const char *url)
{
    if (!url) return CODEC_UNKNOWN;

    probe_ctx_t ctx = {0};

    esp_http_client_config_t cfg = {
        .url               = url,
        .timeout_ms        = 3000,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .user_agent        = "Mozilla/5.0",
        .event_handler     = probe_event_handler,
        .user_data         = &ctx,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return CODEC_UNKNOWN;

    esp_http_client_set_header(client, "Icy-MetaData", "1");
    esp_http_client_set_header(client, "Range", "bytes=0-1");

    codec_type_t codec = CODEC_UNKNOWN;

    esp_err_t err = esp_http_client_open(client, 0);
    if (err == ESP_OK) {
        int len    = esp_http_client_fetch_headers(client);
        int status = esp_http_client_get_status_code(client);

        ESP_LOGI(TAG, "HTTP probe: status=%d len=%d ct=%s",
                 status, len, ctx.content_type[0] ? ctx.content_type : "(null)");

        if (ctx.content_type[0]) {
            if (strcasestr(ctx.content_type, "ogg")) {
                // OGG/Vorbis, OGG/FLAC, OGG/Opus — esp-adf prebuilt has no
                // full support (ogg_decoder = Vorbis only, no OGG/FLAC).
                // Stays CODEC_UNKNOWN → MP3 fallback → decoder error.
                ESP_LOGW(TAG, "OGG container not supported (need RAW stream)");
            } else if (strcasestr(ctx.content_type, "aac")) {
                codec = CODEC_AAC;
            } else if (strcasestr(ctx.content_type, "flac")) {
                codec = CODEC_FLAC;
            } else if (strcasestr(ctx.content_type, "mpeg") ||
                       strcasestr(ctx.content_type, "mp3")) {
                codec = CODEC_MP3;
            }
        }
        esp_http_client_close(client);
    } else {
        ESP_LOGW(TAG, "HTTP probe open failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return codec;
}


/*
static void build_pipeline_for_codec(codec_type_t codec)
Unlinks and re-links the pipeline with a different decoder. Call only when
the pipeline is stopped (after stop+terminate). After relink the listener
must be re-registered, because the newly linked elements don't have it set.
*/
static void build_pipeline_for_codec(codec_type_t codec)
{
    audio_pipeline_unlink(pipeline);

    switch (codec) {
        case CODEC_AAC:
            active_decoder_el = aac_decoder_el;
            break;
        case CODEC_FLAC:
            active_decoder_el = flac_decoder_el;
            break;
        default:
            active_decoder_el = mp3_decoder_el;
            codec = CODEC_MP3;
            break;
    }

    const char *link_tag[5] = { "http", codec_name[codec], "rsp", "dsp", "i2s" };
    audio_pipeline_link(pipeline, link_tag, 5);

    // The listener is attached per-element in audio_player_init via
    // audio_element_msg_set_listener — it stays active after unlink/link,
    // so we don't touch it here. (audio_pipeline_set_listener /
    // remove_listener operate on pipeline->listener which is lost on
    // unlink — that's why we don't use that path.)

    current_codec = codec;
    ESP_LOGI(TAG, "Pipeline linked: http -> %s -> rsp -> dsp -> i2s", codec_name[codec]);
}


/*
void audio_player_init(void)
*/
void audio_player_init(void)
{
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);

    icy_http_stream_cfg_t http_cfg = ICY_HTTP_STREAM_CFG_DEFAULT();
    http_cfg.crt_bundle_attach = esp_crt_bundle_attach;
    http_cfg.event_handle      = http_event_handler;
    // 256 KB ≈ 8 seconds at 256 kbps; absorbs network jitter on weaker
    // WiFi / longer pings. Internal RAM can't fit a buffer this big —
    // esp-adf via audio_calloc falls back to PSRAM (sequential access,
    // negligible performance impact).
    http_cfg.out_rb_size       = 256 * 1024;

    http_stream_reader = icy_http_stream_init(&http_cfg);

    icy_http_stream_set_metadata_cb(http_stream_reader, parse_stream_title);

    // Decoder/rsp/dsp tasks go to core 1 — core 0 is loaded with Wi-Fi/
    // lwIP/http, contention caused minor stutter at 256 kbps despite low CPU.
    mp3_decoder_cfg_t mp3_decoder_cfg = DEFAULT_MP3_DECODER_CONFIG();
    mp3_decoder_cfg.out_rb_size = 32 * 1024;
    mp3_decoder_cfg.task_core   = 1;

    mp3_decoder_el = mp3_decoder_init(&mp3_decoder_cfg);

    aac_decoder_cfg_t aac_decoder_cfg = DEFAULT_AAC_DECODER_CONFIG();
    aac_decoder_cfg.out_rb_size = 32 * 1024;
    aac_decoder_cfg.task_core   = 1;

    aac_decoder_el = aac_decoder_init(&aac_decoder_cfg);

    flac_decoder_cfg_t flac_decoder_cfg = DEFAULT_FLAC_DECODER_CONFIG();
    flac_decoder_cfg.out_rb_size = 32 * 1024;
    flac_decoder_cfg.task_core   = 1;

    flac_decoder_el = flac_decoder_init(&flac_decoder_cfg);

    // Resample filter — upsamples whatever the decoder outputs (22050/32000/48000…)
    // to PLAYBACK_SAMPLE_RATE. This way I2S and DSP can stay on a fixed
    // 44100/16/2, with no race conditions from dynamic i2s_set_clk.
    rsp_filter_cfg_t rsp_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    rsp_cfg.src_rate    = PLAYBACK_SAMPLE_RATE;
    rsp_cfg.src_ch      = PLAYBACK_CHANNELS;
    rsp_cfg.src_bits    = PLAYBACK_BITS;
    rsp_cfg.dest_rate   = PLAYBACK_SAMPLE_RATE;
    rsp_cfg.dest_ch     = PLAYBACK_CHANNELS;
    rsp_cfg.dest_bits   = PLAYBACK_BITS;
    rsp_cfg.complexity  = 2;
    rsp_cfg.out_rb_size = 8 * 1024;
    // Stack in internal SRAM (PSRAM filter caused micro-glitches at higher
    // bitrates — FIR + interpolation = a lot of float math).
    rsp_cfg.stack_in_ext = false;
    rsp_cfg.task_core    = 1;

    rsp_filter_el = rsp_filter_init(&rsp_cfg);

    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();

    i2s_cfg.out_rb_size = 8 * 1024;
    i2s_cfg.type = AUDIO_STREAM_WRITER;

    // GPIO - all in board_pins_config.c

    // clock
    i2s_cfg.std_cfg.clk_cfg.clk_src = I2S_CLK_SRC_DEFAULT;

    // format
    i2s_cfg.std_cfg.slot_cfg.slot_mode = I2S_SLOT_MODE_STEREO;
    i2s_cfg.std_cfg.slot_cfg.data_bit_width = I2S_DATA_BIT_WIDTH_16BIT;
    i2s_cfg.std_cfg.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_16BIT;
    i2s_cfg.std_cfg.slot_cfg.ws_width = I2S_SLOT_BIT_WIDTH_16BIT;
    i2s_cfg.std_cfg.slot_cfg.bit_shift = true;

    // DMA
    i2s_cfg.chan_cfg.dma_desc_num = 6;
    i2s_cfg.chan_cfg.dma_frame_num = 1023;

    i2s_cfg.use_alc = false;

    i2s_stream_writer = i2s_stream_init(&i2s_cfg);

    if (!i2s_stream_writer) {
        ESP_LOGE(TAG, "I2S init failed");
        return;
    }

    // DSP (EQ + volume in a single element)
    dsp_el = audio_dsp_init(44100, 2);

    if (!dsp_el) {
        ESP_LOGE(TAG, "DSP init failed");
        return;
    }

    audio_dsp_set_volume(dsp_el, 0.1f);

    audio_pipeline_register(pipeline, http_stream_reader, "http");
    audio_pipeline_register(pipeline, mp3_decoder_el,     "mp3");
    audio_pipeline_register(pipeline, aac_decoder_el,     "aac");
    audio_pipeline_register(pipeline, flac_decoder_el,    "flac");
    audio_pipeline_register(pipeline, rsp_filter_el,      "rsp");
    audio_pipeline_register(pipeline, dsp_el,             "dsp");
    audio_pipeline_register(pipeline, i2s_stream_writer,  "i2s");

    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    evt = audio_event_iface_init(&evt_cfg);

    // Per-element listener (not per-pipeline). audio_pipeline_set_listener
    // is lost on audio_pipeline_unlink — on codec change events would not
    // reach event_task. Per-element listener survives unlink/link.
    audio_element_msg_set_listener(http_stream_reader, evt);
    audio_element_msg_set_listener(mp3_decoder_el,     evt);
    audio_element_msg_set_listener(aac_decoder_el,     evt);
    audio_element_msg_set_listener(flac_decoder_el,    evt);
    audio_element_msg_set_listener(rsp_filter_el,      evt);
    audio_element_msg_set_listener(dsp_el,             evt);
    audio_element_msg_set_listener(i2s_stream_writer,  evt);

    active_decoder_el = mp3_decoder_el;
    current_codec     = CODEC_MP3;

    const char *link_tag[5] = {"http", "mp3", "rsp", "dsp", "i2s"};
    audio_pipeline_link(pipeline, &link_tag[0], 5);

    // I2S on fixed 44100/16/2 — rsp_filter handles resample from any source.
    i2s_stream_set_clk(i2s_stream_writer,
                       PLAYBACK_SAMPLE_RATE, PLAYBACK_BITS, PLAYBACK_CHANNELS);

    s_play_sem  = xSemaphoreCreateBinary();
    s_play_lock = xSemaphoreCreateMutex();

    xTaskCreate(retry_task,       "audio_retry", 4096, NULL, 4, &s_retry_task_handle);
    xTaskCreate(audio_event_task, "audio_evt",   4096, NULL, 5, NULL);
    // Stack 8 KB — TLS handshake in detect_codec_from_http needs 4-6 KB.
    xTaskCreate(audio_play_task,  "audio_play",  8192, NULL, 5, NULL);
}


/*
static esp_err_t http_event_handler(...)
*/
static int http_event_handler(icy_http_event_msg_t *msg)
{
    if (msg->event_id == ICY_HTTP_EVENT_PRE_REQUEST) {
        ESP_LOGI(TAG, "HTTP request start");
    }
    return ESP_OK;
}


/*
static void sanitize(char *s)
*/
static void sanitize(char *s)
{
    for (int i = 0; s[i]; i++) {
        if (s[i] == '"' || s[i] == '\\') {
            s[i] = ' ';
        }
    }
}


/*
static void parse_stream_title(const char *meta)
*/
static void parse_stream_title(const char *meta)
{
    const char *start = strstr(meta, "StreamTitle='");
    if (!start) return;

    start += strlen("StreamTitle='");

    const char *end = strchr(start, '\'');
    if (!end) return;

    int len = end - start;

    if (len <= 0 || len >= 128) return;

    char title[128];
    memcpy(title, start, len);
    title[len] = 0;

    // Strip end-of-line characters (\r, \n) from the title
    for (int i = 0; i < len; i++) {
        if (title[i] == '\r' || title[i] == '\n') {
            title[i] = ' ';
        }
    }

    // ESP_LOGI(TAG, "NOW PLAYING: %s", title);

    sanitize(title);
    app_state_update(&(app_state_patch_t){
        .has_title = true,
        .title = title
    });
}


/*
void audio_player_play(const char *url)
Lightweight front-end: copies the URL into a shared buffer and wakes
audio_play_task. All the heavy work (HTTP probe, TLS, pipeline rebuild)
happens there — safely away from the WS/httpd task stack.
*/
void audio_player_play(const char *url)
{
    if (!url || !s_play_sem || !s_play_lock) return;

    xSemaphoreTake(s_play_lock, portMAX_DELAY);
    strncpy(s_pending_url, url, PLAY_URL_MAX - 1);
    s_pending_url[PLAY_URL_MAX - 1] = '\0';
    xSemaphoreGive(s_play_lock);

    // Binary semaphore: if the task hasn't handled the previous give yet,
    // this one just gets "marked" and the task will handle the newest URL.
    xSemaphoreGive(s_play_sem);
}


/*
static void audio_play_internal(const char *url)
Actual playback start. Called ONLY from audio_play_task.
*/
static void audio_play_internal(const char *url)
{
    ESP_LOGI(TAG, "Playing URL: %s", url);

    s_retry_count = 0;

    if (is_playing) {
        audio_player_stop();
    }

    // Hybrid detection: URL hint → HTTP probe → MP3 fallback.
    codec_type_t codec = detect_codec_from_url(url);
    if (codec == CODEC_UNKNOWN) {
        ESP_LOGI(TAG, "URL has no codec hint, probing Content-Type...");
        codec = detect_codec_from_http(url);
    }
    if (codec == CODEC_UNKNOWN) {
        ESP_LOGW(TAG, "Codec detection failed, falling back to MP3");
        codec = CODEC_MP3;
    }

    if (codec != current_codec) {
        ESP_LOGI(TAG, "Switching codec: %s -> %s",
                 codec_name[current_codec], codec_name[codec]);
        build_pipeline_for_codec(codec);
    }

    audio_element_set_uri(http_stream_reader, url);
    audio_pipeline_run(pipeline);

    is_playing = true;
}


/*
static void audio_play_task(void *param)
Waits on the semaphore, copies the URL under the mutex, calls
audio_play_internal. Stack 8 KB — enough for esp_http_client + TLS handshake.
*/
static void audio_play_task(void *param)
{
    char url[PLAY_URL_MAX];

    while (1) {
        xSemaphoreTake(s_play_sem, portMAX_DELAY);

        xSemaphoreTake(s_play_lock, portMAX_DELAY);
        strncpy(url, s_pending_url, PLAY_URL_MAX - 1);
        url[PLAY_URL_MAX - 1] = '\0';
        xSemaphoreGive(s_play_lock);

        audio_play_internal(url);
    }
}


/*
void audio_player_stop(void)
*/
void audio_player_stop(void)
{
    if (!is_playing) return;

    audio_pipeline_stop(pipeline);
    audio_pipeline_wait_for_stop(pipeline);
    audio_pipeline_terminate(pipeline);
    audio_pipeline_reset_ringbuffer(pipeline);
    audio_pipeline_reset_elements(pipeline);

    is_playing = false;

    // force rsp reinit on next play (new station may have different sample rate)
    last_rsp_sample_rate = 0;
    last_rsp_channels    = 0;
}


/*
static void retry_task(void *param)
Handles pipeline restart after an HTTP error.
Triggered by xTaskNotifyGive from audio_event_task.
Must be a separate task — restarting the pipeline inside the event task
overflows the event queue and causes a FreeRTOS assert.
*/
static void retry_task(void *param)
{
    // Baseline internal RAM on this board is ~7-8KB largest free block after
    // WiFi/LVGL/audio is loaded. HTTP fits, HTTPS (mbedTLS handshake) needs
    // more — the real fix is CONFIG_MBEDTLS_DYNAMIC_BUFFER in sdkconfig.
    // Here we only log so it's easier to diagnose if something goes badly
    // wrong, but we don't block retry.
    const size_t WARN_INTERNAL_LARGEST = 4 * 1024;

    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (!is_playing) continue;

        // first retry waits longer — TCP/mbedTLS stack needs a moment
        int delay_ms = (s_retry_count <= 1) ? 3000 : 2000;
        vTaskDelay(pdMS_TO_TICKS(delay_ms));

        if (!is_playing) continue;

        // pipeline may have resumed by itself (esp-adf does internal element recovery)
        // — if MUSIC_INFO zeroed the counter, don't break a working stream
        if (s_retry_count == 0) {
            ESP_LOGI(TAG, "Retry cancelled — pipeline recovered");
            continue;
        }

        size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
        if (largest < WARN_INTERNAL_LARGEST) {
            ESP_LOGW(TAG, "Internal RAM critically low (largest=%u) — retry may fail",
                     largest);
        }

        ESP_LOGW(TAG, "Retry %d/%d (internal largest=%u)...",
                 s_retry_count, MAX_RETRIES, largest);

        audio_pipeline_stop(pipeline);
        audio_pipeline_wait_for_stop(pipeline);
        audio_pipeline_terminate(pipeline);
        audio_pipeline_reset_ringbuffer(pipeline);
        audio_pipeline_reset_elements(pipeline);
        audio_pipeline_run(pipeline);

        ESP_LOGI(TAG, "Retry pipeline started");
    }
}


/*
static void audio_event_task(void *param)
*/
static void audio_event_task(void *param)
{
    audio_event_iface_msg_t msg;

    while (1) {
        if (audio_event_iface_listen(evt, &msg, portMAX_DELAY) != ESP_OK) {
            continue;
        }

        // --- MUSIC INFO ---
        // We check active_decoder_el (not mp3_decoder_el), because for AAC/FLAC
        // events come from the currently linked decoder.
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT
            && msg.source == (void *)active_decoder_el
            && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {

            audio_element_info_t music_info = {0};
            audio_element_getinfo(active_decoder_el, &music_info);

            ESP_LOGI(TAG, "[%s] Sample rate: %d, bits: %d, ch: %d, bps: %d, fmt: %d",
                codec_name[current_codec],
                music_info.sample_rates,
                music_info.bits,
                music_info.channels,
                music_info.bps,
                music_info.codec_fmt);

            app_state_update(&(app_state_patch_t){
                .has_audio_info = true,
                .sample_rate    = music_info.sample_rates,
                .bits           = music_info.bits,
                .channels       = music_info.channels,
                .bitrate        = music_info.bps,
                .codec_fmt      = music_info.codec_fmt,
            });

            // pipeline is actually playing — cancel any scheduled retry
            s_retry_count = 0;

            // I2S and DSP are on fixed 44100/16/2. rsp_filter receives the
            // source info and upsamples (or downsamples) to 44100. This
            // eliminates DAC issues at unusual sample rates (e.g. 22050 for
            // AAC) and race conditions with dynamic i2s_stream_set_clk.
            if (music_info.sample_rates != last_rsp_sample_rate ||
                music_info.channels     != last_rsp_channels) {

                last_rsp_sample_rate = music_info.sample_rates;
                last_rsp_channels    = music_info.channels;

                rsp_filter_set_src_info(rsp_filter_el,
                                        music_info.sample_rates,
                                        music_info.channels);
            }
        }

        // --- HTTP ERROR / stream lost → signal to retry_task ---
        // We don't restart the pipeline here — calling stop/terminate inside
        // the event task overflows the event queue (FreeRTOS queue.c:3362 assert).
        //
        // We catch two cases:
        //  1) ERROR_OPEN — HTTP didn't open the stream (e.g. 404, TLS timeout at start)
        //  2) stream-lost — TLS died mid-playback (INPUT_DONE/STATE_FINISHED/ERROR_*).
        //     Internet radio has no "end", so this is always an anomaly → retry.
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT
            && msg.source == (void *)http_stream_reader
            && msg.cmd == AEL_MSG_CMD_REPORT_STATUS) {

            int status = (int)msg.data;

            bool is_open_error  = (status == AEL_STATUS_ERROR_OPEN);
            bool is_stream_lost = is_playing
                && (status == AEL_STATUS_INPUT_DONE
                 || status == AEL_STATUS_STATE_FINISHED
                 || status == AEL_STATUS_ERROR_INPUT
                 || status == AEL_STATUS_ERROR_PROCESS);

            if (is_open_error || is_stream_lost) {
                if (s_retry_count >= MAX_RETRIES) {
                    ESP_LOGE(TAG, "Max retries reached, giving up");
                    s_retry_count = 0;
                    is_playing = false;
                    app_state_update(&(app_state_patch_t){
                        .has_radio   = true,
                        .radio_state = RADIO_STATE_ERROR
                    });
                    continue;
                }

                s_retry_count++;
                ESP_LOGW(TAG, "%s (%d/%d), scheduling retry...",
                         is_open_error ? "HTTP open failed" : "Stream lost",
                         s_retry_count, MAX_RETRIES);

                if (s_retry_task_handle) {
                    xTaskNotifyGive(s_retry_task_handle);
                }
            }
        }
    }
}

void audio_player_set_volume(int volume)
{
    if (!dsp_el) return;

    float vol = volume / 100.0f;

    // Exponent: higher = ramps up more slowly at the start
    // 1.0 = linear, 2.0 = square, 4-5 = typical audio taper
    const float exponent = 4.0f;
    float vol_curved = powf(vol, exponent);

    audio_dsp_set_volume(dsp_el, vol_curved);
}


/*
void audio_player_set_eq_10(int *bands)
*/
void audio_player_set_eq_10(int *bands)
{
    if (!dsp_el) return;

    audio_dsp_set_gains(dsp_el, bands);
}


void audio_player_set_eq_enabled(bool enabled)
{
    if (!dsp_el) return;

    audio_dsp_set_eq_enabled(dsp_el, enabled);
}
