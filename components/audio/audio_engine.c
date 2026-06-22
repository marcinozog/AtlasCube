#include "audio_engine.h"
#include "defines.h"
#include "audio_pipeline.h"
#include "audio_element.h"
#include "audio_dsp.h"
#include "i2s_stream.h"
#include "icy_http_stream.h"
#include "mp3_decoder.h"
#include "aac_decoder.h"
#include "flac_decoder.h"
#include "wav_decoder.h"
#include "fatfs_stream.h"
#include "filter_resample.h"
#include "esp_log.h"
#include "esp_crt_bundle.h"
#include "app_state.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"   // xTaskCreatePinnedToCoreWithCaps
#include "freertos/semphr.h"
#include "esp_heap_caps.h"            // MALLOC_CAP_SPIRAM
#include "esp_timer.h"
#include "heap_report.h"              // internal-RAM diagnostic (temporary)
#include <math.h>
#include <string.h>

static const char *TAG = "AUDIO_ENGINE";

#define PLAY_URI_MAX 512

#define PLAYBACK_SAMPLE_RATE 44100
#define PLAYBACK_CHANNELS    2
#define PLAYBACK_BITS        16

// Index by audio_codec_t (MP3=0 .. WAV=3). UNKNOWN never indexes this.
static const char *codec_name[] = { "mp3", "aac", "flac", "wav" };

static audio_pipeline_handle_t pipeline;
static audio_element_handle_t
    http_stream_reader, mp3_decoder_el, aac_decoder_el, flac_decoder_el,
    fatfs_stream_reader, wav_decoder_el,
    active_decoder_el, rsp_filter_el, dsp_el, i2s_stream_writer;

static audio_event_iface_handle_t evt;

static bool is_playing = false;
static bool is_paused  = false;

// Source + codec currently linked into the pipeline. Drives the relink decision
// on the next play (relink only when one of them changes).
static audio_src_t   current_src   = AUDIO_SRC_HTTP;
static audio_codec_t current_codec = AUDIO_CODEC_MP3;

// Deterministic end-of-file: element STATE_FINISHED events fire when the decoder
// drains, not when the audio has actually played out, so they cut the tail.
// For files whose length we know (WAV header) we time the teardown from the
// clip's own duration (+ a drain margin). 0 = unknown → fall back to events.
static esp_timer_handle_t s_file_end_timer = NULL;
static uint32_t           s_file_duration_ms = 0;
static bool               s_finish_requested = false;   // dedupe end-of-file per playback
static int64_t            s_file_deadline_us = 0;       // when the end timer will fire
static int64_t            s_file_paused_us   = 0;       // remaining end-timer time while paused
#define FILE_END_DRAIN_MARGIN_MS 700

// Resampler reconfig tracking — re-armed whenever the source rate/channels change.
static int last_rsp_sample_rate = 0;
static int last_rsp_channels    = 0;

// Playback start (codec relink + pipeline build) runs on a dedicated
// audio_play_task with an 8 KB stack, off the httpd/WS task (~4 KB) that
// triggers it.
static SemaphoreHandle_t s_play_sem  = NULL;  // binary: trigger
static SemaphoreHandle_t s_play_lock = NULL;  // mutex: protects s_pending_* below
// Serializes every pipeline mutation (start / teardown / restart). The play
// task, the retry path (audio_engine_restart_current) and radio_stop
// (audio_engine_stop) all touch the single pipeline from different tasks;
// without this they can overlap and leave esp_http_client half-freed → crash.
static SemaphoreHandle_t s_pipe_lock = NULL;
static audio_src_t   s_pending_src;
static audio_codec_t s_pending_codec;
static char          s_pending_uri[PLAY_URI_MAX];
static uint32_t      s_pending_duration = 0;
static bool          s_pending_stop   = false;  // set by audio_engine_request_stop()
static bool          s_pending_finish = false;  // file reached its end → stop + cb

// Domain hooks (net/file layers).
static void (*s_info_cb)(void)     = NULL;
static void (*s_error_cb)(int)     = NULL;
static void (*s_file_end_cb)(void) = NULL;

static void audio_event_task(void *param);
static void audio_play_task(void *param);
static int  http_event_handler(icy_http_event_msg_t *msg);
static void engine_relink(audio_src_t src, audio_codec_t codec);
static void engine_start(audio_src_t src, audio_codec_t codec,
                         const char *uri, uint32_t duration_ms);
static void engine_request_finish(void);
static void file_end_timer_cb(void *arg);
static void pipeline_teardown(void);


/*
static int http_event_handler(...)
*/
// Content-Type (esp_codec_type_t) -> our audio_codec_t. Returns
// AUDIO_CODEC_UNKNOWN for codecs we don't decode or an unusable header, so the
// caller keeps the URL-hint guess instead of relinking to nothing.
static audio_codec_t codec_from_ct(esp_codec_type_t ct)
{
    switch (ct) {
        case ESP_CODEC_TYPE_MP3: return AUDIO_CODEC_MP3;
        case ESP_CODEC_TYPE_AAC: return AUDIO_CODEC_AAC;
        case ESP_CODEC_TYPE_WAV: return AUDIO_CODEC_WAV;
        default:                 return AUDIO_CODEC_UNKNOWN;  // OPUS/UNKNOW: no decoder
    }
}

static int http_event_handler(icy_http_event_msg_t *msg)
{
    if (msg->event_id == ICY_HTTP_EVENT_PRE_REQUEST) {
        ESP_LOGI(TAG, "HTTP request start");
        heap_report("pre-connect");   // internal RAM right before TLS open
        return ESP_OK;
    }

    // After headers are parsed the server's Content-Type is the authoritative
    // codec. The pipeline was already linked with the URL-hint guess (MP3 by
    // default); if it was wrong, relink+restart on the same URL. We can't swap a
    // decoder element mid-stream, so this costs one reconnect — but only on a
    // mismatch, and only on the first play of a mis-hinted station.
    if (msg->event_id == ICY_HTTP_EVENT_POST_REQUEST && current_src == AUDIO_SRC_HTTP) {
        audio_codec_t want = codec_from_ct(msg->codec);
        if (want != AUDIO_CODEC_UNKNOWN && want != current_codec) {
            char *uri = audio_element_get_uri(http_stream_reader);
            if (uri) {
                ESP_LOGW(TAG, "Codec mismatch: linked=%s, Content-Type=%s -> relinking",
                         codec_name[current_codec], codec_name[want]);
                // Re-enter through the play front-end: it just marks s_pending_*
                // and wakes audio_play_task, which tears down + relinks under
                // s_pipe_lock. Safe from this (http reader task) callback — same
                // path the retry logic uses. Re-link terminates: the next open
                // reports the same codec, now == current_codec.
                audio_engine_play(AUDIO_SRC_HTTP, want, uri, 0);
            }
        }
    }
    return ESP_OK;
}


/*
static void engine_relink(audio_src_t src, audio_codec_t codec)
Unlinks and re-links the pipeline as <src> -> <codec> -> rsp -> dsp -> i2s.
Call only when the pipeline is stopped. The per-element listener is set once in
audio_engine_init and survives unlink/link, so we don't touch it here
(audio_pipeline_set_listener is lost on unlink — that's why we use per-element).
*/
static void engine_relink(audio_src_t src, audio_codec_t codec)
{
    audio_pipeline_unlink(pipeline);

    switch (codec) {
        case AUDIO_CODEC_AAC:  active_decoder_el = aac_decoder_el;  break;
        case AUDIO_CODEC_FLAC: active_decoder_el = flac_decoder_el; break;
        case AUDIO_CODEC_WAV:  active_decoder_el = wav_decoder_el;  break;
        default:
            active_decoder_el = mp3_decoder_el;
            codec = AUDIO_CODEC_MP3;
            break;
    }

    const char *src_tag = (src == AUDIO_SRC_FILE) ? "file" : "http";
    const char *link_tag[5] = { src_tag, codec_name[codec], "rsp", "dsp", "i2s" };
    audio_pipeline_link(pipeline, link_tag, 5);

    current_src   = src;
    current_codec = codec;

    // New source → make the next MUSIC_INFO reconfigure the resampler.
    last_rsp_sample_rate = 0;
    last_rsp_channels    = 0;

    ESP_LOGI(TAG, "Pipeline linked: %s -> %s -> rsp -> dsp -> i2s",
             src_tag, codec_name[codec]);
}


/*
void audio_engine_init(void)
*/
void audio_engine_init(void)
{
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);

    icy_http_stream_cfg_t http_cfg = ICY_HTTP_STREAM_CFG_DEFAULT();
    http_cfg.crt_bundle_attach = esp_crt_bundle_attach;
    http_cfg.event_handle      = http_event_handler;
    // Player-style (non-browser) UA — broadly compatible with picky
    // SHOUTcast/Icecast servers that treat "Mozilla" clients differently.
    http_cfg.user_agent        = "VLC/3.0.18 LibVLC/3.0.18";
    // 256 KB ≈ 8 seconds at 256 kbps; absorbs network jitter on weaker
    // WiFi / longer pings. Internal RAM can't fit a buffer this big —
    // esp-adf via audio_calloc falls back to PSRAM (sequential access,
    // negligible performance impact).
    http_cfg.out_rb_size       = 256 * 1024;
    // Bound connect/read at 10 s (icy_http_stream defaults to 30 s). A failing
    // or slow HTTPS connect — common when rapid station switches starve internal
    // RAM — would otherwise block the http task (and any teardown waiting on it)
    // for 30 s. 10 s still leaves the 256 KB (~8 s) buffer room to ride out a
    // hiccup. (This timeout is shared by connect and stream read.)
    http_cfg.timeout_ms        = 10 * 1000;
    // Run the HTTP reader's 6 KB task stack in PSRAM. It frees that much
    // internal DRAM and — more importantly — removes the need for a 6 KB
    // *contiguous* internal block at task creation, which was failing under the
    // TLS pressure of rapid HTTPS station switches (two mbedTLS contexts briefly
    // overlap). The reader is I/O-bound on core 0; only the one-off TLS
    // handshake runs slightly slower on the PSRAM stack (the audio-critical
    // rsp/dsp tasks stay in internal SRAM).
    http_cfg.stack_in_ext      = true;

    http_stream_reader = icy_http_stream_init(&http_cfg);
    // ICY StreamTitle callback is wired by the net layer via
    // audio_engine_set_meta_cb (the parser lives there).

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

    // Local-file source (voice notifications + SD music): fatfs reader feeding
    // the same decoders/tail as the radio chain.
    fatfs_stream_cfg_t fatfs_cfg = FATFS_STREAM_CFG_DEFAULT();
    fatfs_cfg.type        = AUDIO_STREAM_READER;
    fatfs_cfg.task_core   = 1;
    // Stack in PSRAM (like the HTTP reader). The file source is I/O-bound (SD
    // reads), so it doesn't need an internal stack — and keeping it out of
    // internal SRAM leaves a contiguous block free for the rsp/dsp stacks, which
    // otherwise failed to allocate when replaying a file after a stop.
    fatfs_cfg.ext_stack   = true;
    fatfs_stream_reader = fatfs_stream_init(&fatfs_cfg);

    wav_decoder_cfg_t wav_decoder_cfg = DEFAULT_WAV_DECODER_CONFIG();
    wav_decoder_cfg.out_rb_size = 32 * 1024;
    wav_decoder_cfg.task_core   = 1;
    wav_decoder_el = wav_decoder_init(&wav_decoder_cfg);

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

    // DMA — these buffers live in internal DMA-capable RAM, the scarcest pool on
    // this board (the same region WiFi RX and the LCD compete for). 6 x 1023
    // frames x 4 B = ~24 KB ≈ 139 ms of I2S buffering — far more than needed
    // given the 8 KB out ringbuffer here and the 256 KB HTTP buffer in PSRAM
    // upstream. 4 x 256 = ~4 KB (~23 ms) frees ~20 KB internal DMA, directly
    // raising the largest contiguous block that starves during an HTTPS connect.
    // Bump frame_num back up if audio underruns appear under load.
    i2s_cfg.chan_cfg.dma_desc_num = 4;
    i2s_cfg.chan_cfg.dma_frame_num = 256;

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
    audio_pipeline_register(pipeline, fatfs_stream_reader,"file");
    audio_pipeline_register(pipeline, mp3_decoder_el,     "mp3");
    audio_pipeline_register(pipeline, aac_decoder_el,     "aac");
    audio_pipeline_register(pipeline, flac_decoder_el,    "flac");
    audio_pipeline_register(pipeline, wav_decoder_el,     "wav");
    audio_pipeline_register(pipeline, rsp_filter_el,      "rsp");
    audio_pipeline_register(pipeline, dsp_el,             "dsp");
    audio_pipeline_register(pipeline, i2s_stream_writer,  "i2s");

    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    evt = audio_event_iface_init(&evt_cfg);

    // Per-element listener (not per-pipeline). audio_pipeline_set_listener
    // is lost on audio_pipeline_unlink — on relink events would not reach
    // event_task. Per-element listener survives unlink/link.
    audio_element_msg_set_listener(http_stream_reader, evt);
    audio_element_msg_set_listener(fatfs_stream_reader, evt);
    audio_element_msg_set_listener(mp3_decoder_el,     evt);
    audio_element_msg_set_listener(aac_decoder_el,     evt);
    audio_element_msg_set_listener(flac_decoder_el,    evt);
    audio_element_msg_set_listener(wav_decoder_el,     evt);
    audio_element_msg_set_listener(rsp_filter_el,      evt);
    audio_element_msg_set_listener(dsp_el,             evt);
    audio_element_msg_set_listener(i2s_stream_writer,  evt);

    active_decoder_el = mp3_decoder_el;
    current_codec     = AUDIO_CODEC_MP3;
    current_src       = AUDIO_SRC_HTTP;

    const char *link_tag[5] = {"http", "mp3", "rsp", "dsp", "i2s"};
    audio_pipeline_link(pipeline, &link_tag[0], 5);

    // I2S on fixed 44100/16/2 — rsp_filter handles resample from any source.
    i2s_stream_set_clk(i2s_stream_writer,
                       PLAYBACK_SAMPLE_RATE, PLAYBACK_BITS, PLAYBACK_CHANNELS);

    s_play_sem  = xSemaphoreCreateBinary();
    s_play_lock = xSemaphoreCreateMutex();
    s_pipe_lock = xSemaphoreCreateMutex();

    xTaskCreate(audio_event_task, "audio_evt",  4096, NULL, 5, NULL);
    // audio_play only orchestrates start/stop/teardown — it never touches flash
    // or runs on the audio hot path, so its 8 KB stack lives in PSRAM to keep
    // contiguous internal SRAM free for the TLS handshake (needs ~6 KB block).
    xTaskCreatePinnedToCoreWithCaps(audio_play_task, "audio_play", 8192, NULL, 5,
                                    NULL, tskNO_AFFINITY, MALLOC_CAP_SPIRAM);
}


/*
void audio_engine_play(...)
Lightweight front-end: copies the request into a shared buffer and wakes
audio_play_task. All the heavy work (relink, HTTP probe, TLS) happens there —
safely away from the WS/httpd task stack.
*/
void audio_engine_play(audio_src_t src, audio_codec_t codec,
                       const char *uri, uint32_t file_duration_ms)
{
    if (!uri || !s_play_sem || !s_play_lock) return;

    xSemaphoreTake(s_play_lock, portMAX_DELAY);
    s_pending_src      = src;
    s_pending_codec    = codec;
    s_pending_duration = file_duration_ms;
    strncpy(s_pending_uri, uri, PLAY_URI_MAX - 1);
    s_pending_uri[PLAY_URI_MAX - 1] = '\0';
    s_pending_stop   = false;   // a new play supersedes a pending stop
    s_pending_finish = false;
    xSemaphoreGive(s_play_lock);

    // Binary semaphore: if the task hasn't handled the previous give yet,
    // this one just gets "marked" and the task will handle the newest request.
    xSemaphoreGive(s_play_sem);
}


/*
void audio_engine_request_stop(void)
Async counterpart to audio_engine_play: marks a stop and wakes audio_play_task,
which runs the (blocking) teardown on its own 8 KB stack.
*/
void audio_engine_request_stop(void)
{
    if (!s_play_sem || !s_play_lock) return;

    xSemaphoreTake(s_play_lock, portMAX_DELAY);
    s_pending_stop   = true;
    s_pending_finish = false;
    xSemaphoreGive(s_play_lock);

    xSemaphoreGive(s_play_sem);
}


/*
static void engine_request_finish(void)
Marks the current file playback as finished and wakes audio_play_task to do the
teardown + source restore. Safe from the esp_timer task or audio_event_task.
*/
static void engine_request_finish(void)
{
    if (current_src != AUDIO_SRC_FILE || s_finish_requested) return;
    s_finish_requested = true;
    if (s_play_lock && s_play_sem) {
        xSemaphoreTake(s_play_lock, portMAX_DELAY);
        s_pending_finish = true;
        xSemaphoreGive(s_play_lock);
        xSemaphoreGive(s_play_sem);
    }
}


static void file_end_timer_cb(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "File end timer → finishing");
    engine_request_finish();
}


/*
static void engine_start(...)
Actual playback start. Called ONLY from audio_play_task. Relinks to the
requested source/codec (only when one changed), sets the URI and runs. For a
known-length file it arms the deterministic end timer.
*/
static void engine_start(audio_src_t src, audio_codec_t codec,
                         const char *uri, uint32_t duration_ms)
{
    ESP_LOGI(TAG, "Playing %s: %s", src == AUDIO_SRC_FILE ? "file" : "url", uri);

    // Caller (audio_play_task) already holds s_pipe_lock — use the unlocked
    // teardown to avoid taking the mutex twice.
    if (is_playing) {
        pipeline_teardown();
    }

    s_file_duration_ms = (src == AUDIO_SRC_FILE) ? duration_ms : 0;
    s_finish_requested = false;

    if (src != current_src || codec != current_codec) {
        engine_relink(src, codec);
    }

    audio_element_handle_t reader =
        (src == AUDIO_SRC_FILE) ? fatfs_stream_reader : http_stream_reader;
    audio_element_set_uri(reader, uri);

    audio_pipeline_run(pipeline);
    is_playing = true;
    is_paused  = false;
    s_file_paused_us = 0;

    // Arm the deterministic end-of-playback timer (duration + drain margin).
    if (src == AUDIO_SRC_FILE && s_file_duration_ms > 0) {
        if (!s_file_end_timer) {
            const esp_timer_create_args_t a = {
                .callback = file_end_timer_cb, .name = "file_end" };
            esp_timer_create(&a, &s_file_end_timer);
        }
        if (s_file_end_timer) {
            uint64_t us = ((uint64_t)s_file_duration_ms + FILE_END_DRAIN_MARGIN_MS) * 1000ULL;
            esp_timer_stop(s_file_end_timer);   // cancel any previous arming
            esp_timer_start_once(s_file_end_timer, us);
            s_file_deadline_us = esp_timer_get_time() + (int64_t)us;
            ESP_LOGI(TAG, "File duration ~%lu ms, end timer armed",
                     (unsigned long)s_file_duration_ms);
        }
    } else if (src == AUDIO_SRC_FILE) {
        ESP_LOGW(TAG, "File duration unknown → relying on STATE_FINISHED event");
    }
}


/*
static void audio_play_task(void *param)
Waits on the semaphore, copies the request under the mutex, dispatches to
finish / stop / start.
*/
static void audio_play_task(void *param)
{
    char uri[PLAY_URI_MAX];

    while (1) {
        xSemaphoreTake(s_play_sem, portMAX_DELAY);

        xSemaphoreTake(s_play_lock, portMAX_DELAY);
        bool stop   = s_pending_stop;   s_pending_stop   = false;
        bool finish = s_pending_finish; s_pending_finish = false;
        audio_src_t   src   = s_pending_src;
        audio_codec_t codec = s_pending_codec;
        uint32_t      dur   = s_pending_duration;
        strncpy(uri, s_pending_uri, PLAY_URI_MAX - 1);
        uri[PLAY_URI_MAX - 1] = '\0';
        xSemaphoreGive(s_play_lock);

        // All pipeline work under s_pipe_lock so a concurrent retry restart or
        // radio_stop can't overlap us mid-teardown/run.
        xSemaphoreTake(s_pipe_lock, portMAX_DELAY);
        if (finish) {
            // A local file reached its end: tear down here (off the event task)
            // and let the file layer restore the previous source. current_src
            // stays FILE so the next radio play relinks back to the http source.
            pipeline_teardown();
        } else if (stop) {
            pipeline_teardown();
        } else {
            engine_start(src, codec, uri, dur);
        }
        xSemaphoreGive(s_pipe_lock);

        // Restore the previous source *outside* the lock — the callback chains
        // into radio_service and may enqueue a new play, which must not run
        // while we hold s_pipe_lock.
        if (finish && s_file_end_cb) s_file_end_cb();
    }
}


/*
static void pipeline_teardown(void)
Actual stop/terminate. The caller MUST hold s_pipe_lock.
*/
static void pipeline_teardown(void)
{
    if (s_file_end_timer) esp_timer_stop(s_file_end_timer);   // cancel pending file-end

    if (!is_playing) return;

    // Resume first if paused — stopping a paused pipeline can wedge.
    if (is_paused) {
        audio_pipeline_resume(pipeline);
        is_paused = false;
    }

    audio_pipeline_stop(pipeline);
    audio_pipeline_wait_for_stop(pipeline);
    audio_pipeline_terminate(pipeline);
    audio_pipeline_reset_ringbuffer(pipeline);
    audio_pipeline_reset_elements(pipeline);

    is_playing = false;
    is_paused  = false;

    // force rsp reinit on next play (new station may have different sample rate)
    last_rsp_sample_rate = 0;
    last_rsp_channels    = 0;
}


/*
void audio_engine_stop(void)
*/
void audio_engine_stop(void)
{
    xSemaphoreTake(s_pipe_lock, portMAX_DELAY);
    pipeline_teardown();
    xSemaphoreGive(s_pipe_lock);
}


/*
void audio_engine_pause(void) / audio_engine_resume(void)
Pause/resume the running pipeline in place (no teardown). The deterministic
file-end timer (WAV) is frozen on pause and re-armed with the remaining time on
resume so a paused clip doesn't "end" while held.
*/
void audio_engine_pause(void)
{
    xSemaphoreTake(s_pipe_lock, portMAX_DELAY);
    if (is_playing && !is_paused) {
        audio_pipeline_pause(pipeline);
        is_paused = true;
        if (s_file_end_timer && s_file_duration_ms > 0) {
            int64_t now = esp_timer_get_time();
            s_file_paused_us = (s_file_deadline_us > now) ? (s_file_deadline_us - now) : 1;
            esp_timer_stop(s_file_end_timer);
        }
    }
    xSemaphoreGive(s_pipe_lock);
}

void audio_engine_resume(void)
{
    xSemaphoreTake(s_pipe_lock, portMAX_DELAY);
    if (is_paused) {
        audio_pipeline_resume(pipeline);
        is_paused = false;
        if (s_file_end_timer && s_file_paused_us > 0) {
            esp_timer_start_once(s_file_end_timer, s_file_paused_us);
            s_file_deadline_us = esp_timer_get_time() + s_file_paused_us;
            s_file_paused_us = 0;
        }
    }
    xSemaphoreGive(s_pipe_lock);
}

bool audio_engine_is_paused(void)
{
    return is_paused;
}


/*
void audio_engine_restart_current(void)
In-place pipeline restart (no relink), used by the radio retry policy. Must NOT
run inside the event-listener task — restarting from there overflows the event
queue and causes a FreeRTOS assert (queue.c:3362).
*/
void audio_engine_restart_current(void)
{
    xSemaphoreTake(s_pipe_lock, portMAX_DELAY);
    if (is_playing) {
        audio_pipeline_stop(pipeline);
        audio_pipeline_wait_for_stop(pipeline);
        audio_pipeline_terminate(pipeline);
        audio_pipeline_reset_ringbuffer(pipeline);
        audio_pipeline_reset_elements(pipeline);
        audio_pipeline_run(pipeline);
    }
    xSemaphoreGive(s_pipe_lock);
}


void audio_engine_mark_stopped(void)
{
    is_playing = false;
    last_rsp_sample_rate = 0;
    last_rsp_channels    = 0;
}


bool audio_engine_is_playing(void)
{
    return is_playing;
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
        // We check active_decoder_el (not mp3_decoder_el), because for AAC/FLAC/
        // WAV events come from the currently linked decoder.
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

            // Decoder is producing output → playback is healthy. Let the net
            // layer cancel any scheduled retry.
            if (s_info_cb) s_info_cb();

            heap_report("mid-stream");   // internal RAM once audio is flowing

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

        // --- LOCAL FILE end fallback (only when duration is unknown) ---
        // Normally a known-length file's end is handled deterministically by the
        // duration timer (element STATE_FINISHED fires when the decoder drains,
        // before the audio has played out, so it would cut the tail). This event
        // path is the safety net for files whose length we couldn't read
        // up-front (MP3/FLAC/AAC, and WAVs with an unparseable header).
        if (current_src == AUDIO_SRC_FILE && !s_finish_requested
            && s_file_duration_ms == 0
            && msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT
            && msg.cmd == AEL_MSG_CMD_REPORT_STATUS
            && msg.source == (void *)i2s_stream_writer
            && (int)msg.data == AEL_STATUS_STATE_FINISHED) {

            ESP_LOGI(TAG, "File STATE_FINISHED → finishing");
            engine_request_finish();
            continue;
        }

        // --- HTTP reader status → net layer's retry policy ---
        // We don't act on it here — calling stop/terminate inside the event task
        // overflows the event queue (FreeRTOS queue.c:3362 assert). The net layer
        // classifies the AEL_STATUS_* code and (if needed) drives retry via the
        // dedicated retry task.
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT
            && msg.source == (void *)http_stream_reader
            && msg.cmd == AEL_MSG_CMD_REPORT_STATUS) {

            if (s_error_cb) s_error_cb((int)msg.data);
        }
    }
}


void audio_engine_set_volume(int volume)
{
    if (!dsp_el) return;

    float vol = volume / 100.0f;

    // Exponent: higher = ramps up more slowly at the start
    // 1.0 = linear, 2.0 = square, 4-5 = typical audio taper
    const float exponent = 4.0f;
    float vol_curved = powf(vol, exponent);

    audio_dsp_set_volume(dsp_el, vol_curved);
}


void audio_engine_set_eq_10(int *bands)
{
    if (!dsp_el) return;
    audio_dsp_set_gains(dsp_el, bands);
}


void audio_engine_set_eq_enabled(bool enabled)
{
    if (!dsp_el) return;
    audio_dsp_set_eq_enabled(dsp_el, enabled);
}


void audio_engine_set_meta_cb(void (*cb)(const char *icy_meta))
{
    if (http_stream_reader) {
        icy_http_stream_set_metadata_cb(http_stream_reader, cb);
    }
}


void audio_engine_set_info_cb(void (*cb)(void))      { s_info_cb = cb; }
void audio_engine_set_error_cb(void (*cb)(int))      { s_error_cb = cb; }
void audio_engine_set_file_end_cb(void (*cb)(void))  { s_file_end_cb = cb; }
