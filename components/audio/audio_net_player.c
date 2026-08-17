#include "audio_net_player.h"
#include "audio_engine.h"
#include "audio_element.h"      // AEL_STATUS_* status codes
#include "app_state.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"   // xTaskCreatePinnedToCoreWithCaps
#include <string.h>
#include <strings.h>

static const char *TAG = "AUDIO_NET";

#define MAX_RETRIES 5

// How long a stream has to have been producing audio for its loss to count as a
// fresh incident rather than another failure in the same sequence.
//
// The budget exists for streams that never work — a dead URL, no route, an
// internal-RAM shortage that kills the TLS read a second after it opens. Those
// fail within a couple of seconds of the open, so five of them burn the budget
// and the device reports an error instead of restarting the pipeline forever.
// A station that played for half a minute and then dropped is the other thing
// entirely: that is the network, and internet radio is supposed to keep
// reconnecting to it. So it gets its five attempts back.
#define STABLE_PLAY_US  (30 * 1000000LL)

static TaskHandle_t s_retry_task_handle = NULL;
static int s_retry_count = 0;
static bool s_finite = false;   // current stream is a finite episode, not radio

// A retry was scheduled and has not run yet. Separate from the budget because
// the two answer different questions: this one is "is there a restart pending",
// the counter is "how many have we already spent". Folding them together is what
// made the budget unusable — the cancellation path had to zero the counter, and
// it zeroed it on every successful start, so MAX_RETRIES was never reached.
static bool s_retry_pending = false;

// esp_timer_get_time() at the last MUSIC_INFO, i.e. when the current attempt
// started producing audio. 0 = this attempt never got that far (a failed open,
// or a restart that has not connected yet).
static int64_t s_playing_since_us = 0;

static void on_meta(const char *meta);
static void on_info(void);
static void on_error(int status);
static void retry_task(void *param);


/*
static audio_codec_t detect_codec_from_url(const char *url)
Quick URL-based detection — no HTTP request. Returns AUDIO_CODEC_UNKNOWN when the
URL has no readable hint (typical SHOUTcast/Icecast endpoints without an
extension, e.g. /1, /stream); the caller then defaults to MP3.
*/
static audio_codec_t detect_codec_from_url(const char *url)
{
    if (!url) return AUDIO_CODEC_UNKNOWN;

    // .flac must include the dot — bare "flac" in a stream name may mean
    // OGG/FLAC (audio/ogg, e.g. juventus_FLAC), which is not supported,
    // so without the dot we go to probe instead of assuming FLAC.
    if (strcasestr(url, ".flac")) {
        return AUDIO_CODEC_FLAC;
    }

    // HLS (.m3u8): AAC is the common case, so link it up front. If the TS PMT
    // says the segments carry MP3 instead, icy_http_stream reports it in the
    // POST_REQUEST event and the engine relinks to the MP3 decoder.
    if (strcasestr(url, ".m3u8")) {
        return AUDIO_CODEC_AAC;
    }

    if (strcasestr(url, ".aac")  ||
        strcasestr(url, ".aacp") ||
        strcasestr(url, ".adts") ||
        strcasestr(url, "aacp")  ||   // e.g. /stream.aacp or /live_aacp
        strcasestr(url, "/aac")) {    // e.g. /stream/aac/128
        return AUDIO_CODEC_AAC;
    }

    if (strcasestr(url, ".mp3")) {
        return AUDIO_CODEC_MP3;
    }

    return AUDIO_CODEC_UNKNOWN;
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
static void on_meta(const char *meta)
ICY StreamTitle parser, registered with the engine as the metadata callback.
*/
static void on_meta(const char *meta)
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

    sanitize(title);
    app_state_update(&(app_state_patch_t){
        .has_title = true,
        .title = title
    });
}


/*
static void on_info(void)
A decoder reported MUSIC_INFO → the stream is producing output. Cancel any
scheduled retry (esp-adf may have recovered the HTTP element internally) and
start the clock that decides whether this attempt counts as healthy.

Deliberately does NOT clear the retry budget. MUSIC_INFO fires a second or two
after the connect, long before the stream has proven anything — when the failure
is a TLS read dying just after the decoder reads the sample rate, this callback
runs on every single attempt and would hand back a full budget each time.
*/
static void on_info(void)
{
    s_retry_pending    = false;
    s_playing_since_us = esp_timer_get_time();
}


/*
static void on_error(int status)
HTTP reader status from the engine. Internet radio has no legitimate "end", so
both an open failure and a mid-stream loss are anomalies → schedule a retry.
  1) ERROR_OPEN — HTTP didn't open (e.g. 404, TLS timeout at start).
  2) stream-lost — TLS died mid-playback (INPUT_DONE/STATE_FINISHED/ERROR_*).
*/
static void on_error(int status)
{
    bool is_open_error  = (status == AEL_STATUS_ERROR_OPEN);
    bool is_stream_lost = audio_engine_is_playing()
        && (status == AEL_STATUS_INPUT_DONE
         || status == AEL_STATUS_STATE_FINISHED
         || status == AEL_STATUS_ERROR_INPUT
         || status == AEL_STATUS_ERROR_PROCESS);

    if (!is_open_error && !is_stream_lost) return;

    // Finite stream (podcast episode): the end of the stream is success, not an
    // anomaly. Any stream-end/loss ends playback cleanly with no retry; only a
    // failed open still retries (episode URL may be slow/redirected on first
    // hit). A mid-episode WiFi drop also lands here → reported as finished;
    // Phase-2 resume (Range request) will refine this.
    if (s_finite && is_stream_lost) {
        ESP_LOGI(TAG, "Finite stream ended (status=%d)", status);
        s_retry_count      = 0;
        s_retry_pending    = false;
        s_playing_since_us = 0;
        audio_engine_mark_stopped();
        app_state_update(&(app_state_patch_t){
            .has_radio   = true,
            .radio_state = RADIO_STATE_FINISHED
        });
        return;
    }

    // Did this attempt play long enough to earn a fresh budget? Asked before the
    // cap, so a station that ran for a while and then dropped never trips it.
    int64_t played_us = s_playing_since_us
        ? esp_timer_get_time() - s_playing_since_us : 0;

    if (played_us > STABLE_PLAY_US) {
        ESP_LOGI(TAG, "Stream ran %lld s before dropping — retry budget reset",
                 played_us / 1000000);
        s_retry_count = 0;
    }
    s_playing_since_us = 0;   // the next attempt has not produced audio yet

    if (s_retry_count >= MAX_RETRIES) {
        ESP_LOGE(TAG, "Max retries reached, giving up");
        s_retry_count   = 0;
        s_retry_pending = false;   // nothing is owed a restart any more
        audio_engine_mark_stopped();
        app_state_update(&(app_state_patch_t){
            .has_radio   = true,
            .radio_state = RADIO_STATE_ERROR
        });
        return;
    }

    s_retry_count++;
    s_retry_pending = true;
    ESP_LOGW(TAG, "%s (%d/%d), scheduling retry...",
             is_open_error ? "HTTP open failed" : "Stream lost",
             s_retry_count, MAX_RETRIES);

    if (s_retry_task_handle) {
        xTaskNotifyGive(s_retry_task_handle);
    }
}


/*
static void retry_task(void *param)
Restarts the pipeline after an HTTP error. Triggered by xTaskNotifyGive from
on_error (which runs on the engine's event task). Must be a separate task —
restarting from inside the event listener overflows the event queue and causes
a FreeRTOS assert.
*/
static void retry_task(void *param)
{
    // Internal RAM on this board runs tight enough that a retry can fail for want
    // of memory rather than network, so it is worth having in the log. Reported
    // over the DMA-capable subset, not MALLOC_CAP_INTERNAL: RTCRAM counts as
    // internal but cannot be DMAed into, so the plain internal figure has read up
    // to 8 KB high — it said 17408 while the block that actually mattered was
    // 1728. Diagnostic only; a low reading never blocks the retry.
    //
    // (The previous note here pointed at CONFIG_MBEDTLS_DYNAMIC_BUFFER as "the
    // real fix". It is not: sdkconfig.defaults documents that DYNAMIC_BUFFER
    // breaks HTTPS against a renegotiating CDN.)
    const size_t WARN_DMA_LARGEST = 4 * 1024;

    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (!audio_engine_is_playing()) continue;

        // first retry waits longer — TCP/mbedTLS stack needs a moment
        int delay_ms = (s_retry_count <= 1) ? 3000 : 2000;
        vTaskDelay(pdMS_TO_TICKS(delay_ms));

        if (!audio_engine_is_playing()) continue;

        // pipeline may have resumed by itself (esp-adf does internal element
        // recovery) — on_info() clears the pending flag, so don't restart a
        // stream that came back on its own.
        if (!s_retry_pending) {
            ESP_LOGI(TAG, "Retry cancelled — pipeline recovered");
            continue;
        }

        size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL |
                                                          MALLOC_CAP_DMA);
        if (largest < WARN_DMA_LARGEST) {
            ESP_LOGW(TAG, "DMA-capable RAM critically low (largest=%u) — retry may fail",
                     largest);
        }

        ESP_LOGW(TAG, "Retry %d/%d (DMA largest=%u)...",
                 s_retry_count, MAX_RETRIES, largest);

        audio_engine_restart_current();

        ESP_LOGI(TAG, "Retry pipeline started");
    }
}


void audio_net_player_init(void)
{
    // Retry orchestration only (waits on a notify, then restarts the pipeline) —
    // off the audio hot path and no flash access, so its stack goes to PSRAM to
    // spare contiguous internal SRAM for the TLS handshake.
    xTaskCreatePinnedToCoreWithCaps(retry_task, "audio_retry", 4096, NULL, 4,
                                    &s_retry_task_handle, tskNO_AFFINITY,
                                    MALLOC_CAP_SPIRAM);

    audio_engine_set_meta_cb(on_meta);
    audio_engine_set_info_cb(on_info);
    audio_engine_set_error_cb(on_error);
}


void audio_net_player_play(const char *url, bool finite, uint32_t offset_bytes)
{
    if (!url) return;

    // A deliberate play is a clean slate: full budget, nothing scheduled, and no
    // playback time carried over from whatever was on before.
    s_retry_count      = 0;
    s_retry_pending    = false;
    s_playing_since_us = 0;
    s_finite = finite;

    // Codec from URL hint; extension-less URLs (e.g. SHOUTcast host:port/;)
    // default to MP3 — the vast majority of such streams are MP3, and a
    // pre-flight HTTP probe is unreliable against ancient SHOUTcast servers.
    audio_codec_t codec = detect_codec_from_url(url);
    if (codec == AUDIO_CODEC_UNKNOWN) {
        ESP_LOGI(TAG, "No codec hint in URL, defaulting to MP3");
        codec = AUDIO_CODEC_MP3;
    }

    // Byte offset for a resuming podcast (0 for radio / play-from-start). Set
    // before the async play so the request task reads it at open time.
    audio_engine_set_http_offset(offset_bytes);

    audio_engine_play(AUDIO_SRC_HTTP, codec, url, 0);
}
