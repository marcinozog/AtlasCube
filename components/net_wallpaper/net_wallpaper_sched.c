#include "net_wallpaper_sched.h"
#include "net_wallpaper.h"
#include "net_asset.h"      // assets share this schedule and this batch
#include "settings.h"
#include "ntp_service.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <time.h>

static const char *TAG = "NET_WP_SCHED";

// Short on purpose: the boot batch now runs BEFORE the radio autostart (the
// caller defers radio_resume_on_boot() until net_wallpaper_sched_boot_done_cb
// fires), so there is no playing stream to let settle — only DHCP/DNS to catch
// up with the link-up we were just told about.
#define BOOT_FETCH_DELAY_S   3
#define NTP_RECHECK_DELAY_S  (5 * 60)    // daily mode armed before the clock is valid
#define RETRY_DELAY_S        (15 * 60)
#define RETRY_MAX            4

enum { MODE_OFF = 0, MODE_BOOT = 1, MODE_DAILY = 2 };

// All state transitions run under s_lock; contexts involved are rare and
// short-lived (boot init, a settings POST, the timer callback, the tail of a
// fetch task), so a plain mutex is enough.
static SemaphoreHandle_t  s_lock;
static esp_timer_handle_t s_timer;          // exists only while an event is pending
static int  s_panel_w, s_panel_h;
static bool s_inited;
static bool s_boot_pending;    // the one-time post-boot fetch hasn't fired yet
static bool s_fire_is_fetch;   // meaning of the armed timer: fetch vs re-check
static bool s_sched_fetch;     // a scheduler-kicked fetch is in flight
static int  s_retries;

// The boot batch gates the radio autostart: the owner of that decision (main/
// network_services.c) registers a callback here and holds playback until it
// fires. It fires EXACTLY ONCE, on the first completion of the boot batch —
// success or failure alike, and without waiting for the 15-minute retry, since
// music must not be hostage to a wallpaper that will not download.
static void (*s_boot_done_cb)(void);
static bool  s_boot_cb_armed;  // the pending callback belongs to the boot fetch

static void timer_cb(void *arg);

// Fire the boot gate once, then forget it. Caller holds s_lock; the callback
// itself runs OUTSIDE the lock (see the call sites) because it starts playback.
static void (*take_boot_cb(void))(void)
{
    if (!s_boot_cb_armed) return NULL;
    s_boot_cb_armed = false;
    return s_boot_done_cb;
}

// (Re)arm the single one-shot; the timer object is created on demand.
static void arm(uint64_t delay_s, bool is_fetch)
{
    if (!s_timer) {
        const esp_timer_create_args_t args = {
            .callback = timer_cb,
            .name     = "net_wp_sched",
        };
        if (esp_timer_create(&args, &s_timer) != ESP_OK) {
            ESP_LOGE(TAG, "timer create failed");
            return;
        }
    }
    esp_timer_stop(s_timer);   // harmless when not running
    s_fire_is_fetch = is_fetch;
    esp_timer_start_once(s_timer, delay_s * 1000000ULL);
}

// Idle = stopped, not deleted: esp_timer_delete can race an in-flight callback
// of the same timer (the dispatcher still touches the timer struct after the
// callback returns), and closing that race costs more state than the one small
// handle it would free. A stopped one-shot fires nothing and wakes nothing;
// the handle is created lazily on first use and lives for the session.
static void disarm(void)
{
    if (s_timer) esp_timer_stop(s_timer);
}

// Seconds until the next HH:MM in local time (TZ applied by ntp_service).
static uint64_t seconds_to_next(int hour, int min)
{
    time_t now = time(NULL);
    struct tm lt;
    localtime_r(&now, &lt);
    struct tm tgt = lt;
    tgt.tm_hour = hour;
    tgt.tm_min  = min;
    tgt.tm_sec  = 0;
    time_t t = mktime(&tgt);
    if (t <= now) t += 24 * 3600;   // recomputed after each fire, so a DST shift self-corrects next day
    return (uint64_t)(t - now);
}

// Single decision point: looks at settings + state and arms exactly one next
// event (or none). Caller holds s_lock.
static void schedule_next(void)
{
    const app_settings_t *st = settings_get();
    const int mode = st->display.wallpaper_fetch_mode;

    // An asset URL counts as work too — see boot_fetch_due().
    bool any_url = net_asset_url_count() > 0;
    for (int i = 0; !any_url && i < WALLPAPER_SLOTS; i++)
        if (st->display.wallpaper_url[i][0]) any_url = true;

    if (!s_inited || mode == MODE_OFF || !any_url) {
        disarm();
        return;
    }
    if (s_boot_pending) {                    // covers both BOOT and DAILY modes
        arm(BOOT_FETCH_DELAY_S, true);
        return;
    }
    if (mode == MODE_DAILY) {
        if (!ntp_service_is_synced()) {      // can't aim at a wall-clock time yet
            arm(NTP_RECHECK_DELAY_S, false);
            return;
        }
        uint64_t d = seconds_to_next(st->display.wallpaper_fetch_hour,
                                     st->display.wallpaper_fetch_min);
        ESP_LOGI(TAG, "next daily fetch at %02d:%02d (in %llu s)",
                 st->display.wallpaper_fetch_hour, st->display.wallpaper_fetch_min,
                 (unsigned long long)d);
        arm(d, true);
        return;
    }
    disarm();   // MODE_BOOT with the boot fetch behind us — done until reboot
}

// Returns a boot callback to fire when the fetch could not even be started —
// otherwise a refused kick would hold the radio forever. Caller holds s_lock and
// fires the result after releasing it.
static void (*kick_fetch(void))(void)
{
    s_sched_fetch = true;
    // Every configured slot in one batch (one radio-stop window), not one URL.
    if (net_wallpaper_fetch_all(s_panel_w, s_panel_h)) return NULL;

    // Refused — a manual fetch is already running. Its completion goes
    // through net_wallpaper_sched_fetch_done, which re-arms; nothing lost.
    s_sched_fetch = false;
    // …but that other fetch is not ours, so nothing will report the boot batch
    // as finished. Release the radio now rather than never.
    return take_boot_cb();
}

static void timer_cb(void *arg)
{
    (void)arg;
    void (*boot_cb)(void) = NULL;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_fire_is_fetch) {
        s_boot_pending = false;
        boot_cb = kick_fetch();              // brief: spawns the fetch task
    } else {
        schedule_next();                     // NTP re-check
    }
    xSemaphoreGive(s_lock);
    if (boot_cb) boot_cb();
}

// Is a post-boot batch actually going to run? Mirrors schedule_next()'s own
// gate, because "the feature is on" is not enough — a device with the mode set
// but no URLs must not hold the radio waiting for a fetch that never happens.
static bool boot_fetch_due(void)
{
    const app_settings_t *st = settings_get();
    if (st->display.wallpaper_fetch_mode == MODE_OFF) return false;
    for (int i = 0; i < WALLPAPER_SLOTS; i++)
        if (st->display.wallpaper_url[i][0]) return true;
    // Assets ride the same batch, so a device with only asset URLs still has a
    // fetch coming — and the radio gate must wait for it like any other.
    return net_asset_url_count() > 0;
}

void net_wallpaper_sched_set_boot_done_cb(void (*cb)(void))
{
    s_boot_done_cb = cb;
}

bool net_wallpaper_sched_boot_fetch_pending(void)
{
    return s_inited && s_boot_cb_armed;
}

void net_wallpaper_sched_init(int panel_w, int panel_h)
{
    if (s_inited) return;
    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) return;
    s_panel_w = panel_w;
    s_panel_h = panel_h;
    s_inited  = true;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_boot_pending  = settings_get()->display.wallpaper_fetch_mode != MODE_OFF;
    // Arm the radio gate only when a batch is genuinely coming, and only if
    // someone registered for it — otherwise the caller starts playback at once.
    s_boot_cb_armed = s_boot_done_cb && boot_fetch_due();
    schedule_next();
    xSemaphoreGive(s_lock);

    if (s_boot_cb_armed)
        ESP_LOGI(TAG, "boot batch first, radio autostart deferred until it ends");
}

void net_wallpaper_sched_update(void)
{
    if (!s_inited) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_retries = 0;                           // a config change resets the retry budget
    schedule_next();
    xSemaphoreGive(s_lock);
}

void net_wallpaper_sched_fetch_done(bool ok)
{
    if (!s_inited) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    const bool scheduled = s_sched_fetch;
    s_sched_fetch = false;

    // The boot batch is over either way — a retry 15 minutes from now must not
    // keep the radio silent until then, so the gate opens on this first result.
    void (*boot_cb)(void) = take_boot_cb();

    if (scheduled && !ok && s_retries < RETRY_MAX &&
        settings_get()->display.wallpaper_fetch_mode != MODE_OFF) {
        s_retries++;
        ESP_LOGW(TAG, "scheduled fetch failed (%s) — retry %d/%d in %d min",
                 net_wallpaper_status(), s_retries, RETRY_MAX, RETRY_DELAY_S / 60);
        arm(RETRY_DELAY_S, true);
    } else {
        if (scheduled) s_retries = 0;
        schedule_next();                     // manual fetches land here too: harmless recompute
    }
    xSemaphoreGive(s_lock);

    // Outside the lock: this starts playback, which is not work to do while
    // holding the scheduler's mutex.
    if (boot_cb) boot_cb();
}
