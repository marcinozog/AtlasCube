#include "ui_manager.h"
#include "ui_screen.h"
#include "ui_events.h"
#include "theme.h"
#include "app_state.h"
#include "wifi_manager.h"
#include "screen_event_notification.h"
#include "events_service.h"
#include "screensavers.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

// --------------------------------------------------------------------------
// Screen forward declarations — add every new screen here
// --------------------------------------------------------------------------

extern const ui_screen_t screen_splash;
extern const ui_screen_t screen_radio;
extern const ui_screen_t screen_playlist;
extern const ui_screen_t screen_clock;
// extern const ui_screen_t screen_weather;
extern const ui_screen_t screen_bt;
extern const ui_screen_t screen_wifi;
extern const ui_screen_t screen_settings;
extern const ui_screen_t screen_equalizer;
extern const ui_screen_t screen_events;
extern const ui_screen_t screen_event_notification;

// --------------------------------------------------------------------------
// Screen registry — order must match ui_screen_id_t
// --------------------------------------------------------------------------

static const ui_screen_t *s_screens[SCREEN_COUNT] = {
    [SCREEN_SPLASH]             = &screen_splash,
    [SCREEN_RADIO]              = &screen_radio,
    [SCREEN_PLAYLIST]           = &screen_playlist,
    [SCREEN_CLOCK]              = &screen_clock,
    [SCREEN_WEATHER]            = NULL,
    [SCREEN_BT]                 = &screen_bt,
    [SCREEN_WIFI]               = &screen_wifi,
    [SCREEN_SETTINGS]           = &screen_settings,
    [SCREEN_EQ]                 = &screen_equalizer,
    [SCREEN_EVENTS]             = &screen_events,
    [SCREEN_EVENT_NOTIFICATION] = &screen_event_notification,
};

// --------------------------------------------------------------------------
// Manager state
// --------------------------------------------------------------------------

static const char *TAG = "UI_MGR";

#define UI_EVENT_QUEUE_SIZE 16

static QueueHandle_t        s_event_queue = NULL;
static ui_screen_id_t       s_active_id   = SCREEN_COUNT; // sentinel: none
static const ui_screen_t   *s_active      = NULL;

static bool s_prev_bt_enable = false;
static ui_theme_t     s_prev_theme    = THEME_DARK;

// Screensaver overlay — runs on top of an existing screen.
// While active, the underlying s_active widgets are torn down but s_active /
// s_active_id still point at the underlying screen so we can rebuild it.
static const ui_screen_t *s_ss_overlay     = NULL;
static int64_t            s_last_input_us  = 0;

static bool can_auto_screensaver_from(ui_screen_id_t id)
{
    if (id >= SCREEN_COUNT)              return false;
    if (id == SCREEN_SPLASH)             return false;
    if (id == SCREEN_EVENT_NOTIFICATION) return false;
    return true;
}

// --------------------------------------------------------------------------
// app_state callback — invoked from a foreign task, only pushes an event
// --------------------------------------------------------------------------

static void on_state_change(void)
{
    app_state_t *s = app_state_get();

    if (s->theme != s_prev_theme) {
        ESP_LOGI("UI_MGR", "Theme change: %d → %d", (int)s_prev_theme, (int)s->theme);
        s_prev_theme = s->theme;
        // theme_set() was already called by settings_set_theme() — only the event here
        ui_event_t ev = { .type = UI_EVT_THEME_CHANGED };
        ui_event_send(&ev);
        return;
    }

    if (wifi_get_run_mode() == WIFI_RUN_MODE_AP) return;

    ui_event_t ev = { .type = UI_EVT_STATE_CHANGED };
    ui_event_send(&ev);
}

// --------------------------------------------------------------------------
// Navigation — executed ONLY inside ui_manager_run (lvgl_task)
// --------------------------------------------------------------------------

// Rebuild the current screen — used after a ui_profile (layout) change from
// the web. We keep s_active_id, but destroy and recreate the widgets, because
// values from ui_profile_get() are copied during create().
static void do_rebuild_active(void)
{
    if (s_ss_overlay) {
        // Underlying widgets aren't built right now; they will be rebuilt with
        // the latest profile when the screensaver is dismissed.
        return;
    }
    if (!s_active) {
        ESP_LOGW(TAG, "rebuild: no active screen");
        return;
    }
    if (s_active->destroy) s_active->destroy();
    lv_obj_clean(lv_scr_act());
    if (s_active->create) {
        ESP_LOGI(TAG, "rebuild: %s", s_active->name ? s_active->name : "?");
        s_active->create(lv_scr_act());
    }
}

// Tear down whichever widget tree is currently on screen (overlay or screen).
static void teardown_displayed(void)
{
    if (s_ss_overlay) {
        if (s_ss_overlay->destroy) s_ss_overlay->destroy();
        s_ss_overlay = NULL;
    } else if (s_active && s_active->destroy) {
        ESP_LOGI(TAG, "destroy: %s", s_active->name ? s_active->name : "?");
        s_active->destroy();
    }
    lv_obj_clean(lv_scr_act());
}

static void do_navigate(ui_screen_id_t id)
{
    if (id >= SCREEN_COUNT) {
        ESP_LOGW(TAG, "Invalid screen_id: %d", id);
        return;
    }

    if (!s_screens[id]) {
        ESP_LOGW(TAG, "Screen %d not implemented yet", id);
        return;
    }

    teardown_displayed();

    s_active_id = id;
    s_active    = s_screens[id];

    if (s_active->create) {
        ESP_LOGI(TAG, "create: %s", s_active->name ? s_active->name : "?");
        s_active->create(lv_scr_act());
    }
}

static void activate_screensaver(int ss_id)
{
    if (s_ss_overlay) return;

    const ui_screen_t *ss = screensaver_get(ss_id);
    if (!ss) return;

    // Tear down the underlying screen's widgets but keep s_active pointer
    // so we can rebuild it on dismiss.
    if (s_active && s_active->destroy) s_active->destroy();
    lv_obj_clean(lv_scr_act());

    s_ss_overlay = ss;
    ESP_LOGI(TAG, "screensaver activate: %s", ss->name ? ss->name : "?");
    if (ss->create) ss->create(lv_scr_act());
}

static void dismiss_screensaver(void)
{
    if (!s_ss_overlay) return;

    if (s_ss_overlay->destroy) s_ss_overlay->destroy();
    ESP_LOGI(TAG, "screensaver dismiss → %s",
             s_active && s_active->name ? s_active->name : "?");
    s_ss_overlay = NULL;
    lv_obj_clean(lv_scr_act());

    if (s_active && s_active->create) s_active->create(lv_scr_act());
}

// --------------------------------------------------------------------------
// Public API
// --------------------------------------------------------------------------

// Bridge from events_service: fire → push ui_event_t onto the queue.
// Called from the esp_timer task — only non-blocking operations allowed.
static void on_event_fired(const event_t *e)
{
    ui_event_t uie = { .type = UI_EVT_EVENT_FIRED };
    strncpy(uie.event_info.id,    e->id,    sizeof(uie.event_info.id)    - 1);
    strncpy(uie.event_info.title, e->title, sizeof(uie.event_info.title) - 1);
    uie.event_info.hour   = e->hour;
    uie.event_info.minute = e->minute;
    strncpy(uie.event_info.type_label, events_type_label(e->type),
            sizeof(uie.event_info.type_label) - 1);
    ui_event_send(&uie);
}

void ui_manager_init(void)
{
    s_event_queue = xQueueCreate(UI_EVENT_QUEUE_SIZE, sizeof(ui_event_t));
    configASSERT(s_event_queue);

    app_state_subscribe(on_state_change);
    events_service_set_fire_cb(on_event_fired);

    ESP_LOGI(TAG, "Initialized");
}

void ui_navigate(ui_screen_id_t screen_id)
{
    ui_event_t ev = {
        .type      = UI_EVT_NAVIGATE,
        .screen_id = screen_id
    };
    ui_event_send(&ev);
}

void ui_event_send(const ui_event_t *ev)
{
    if (!s_event_queue) return;
    // don't block — if queue is full, the event is dropped (could add a log)
    if (xQueueSend(s_event_queue, ev, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Event queue full, dropping type=%d", ev->type);
    }
}

void ui_event_send_from_isr(const ui_event_t *ev)
{
    if (!s_event_queue) return;
    BaseType_t woken = pdFALSE;
    xQueueSendFromISR(s_event_queue, ev, &woken);
    portYIELD_FROM_ISR(woken);
}

void ui_input_send(ui_input_t input)
{
    ui_event_t ev = {
        .type  = UI_EVT_INPUT,
        .input = input,
    };
    ui_event_send(&ev);
}

// --------------------------------------------------------------------------
// Main loop — the only place we touch LVGL
// --------------------------------------------------------------------------

void ui_manager_run(void)
{
    s_last_input_us = esp_timer_get_time();
    do_navigate(SCREEN_SPLASH);

    ui_event_t ev;

    while (1) {
        // Cap at N events per cycle — prevents starving lv_timer_handler
        int processed = 0;
        while (processed < 8 && xQueueReceive(s_event_queue, &ev, 0) == pdTRUE) {
            processed++;

            if (ev.type == UI_EVT_NAVIGATE) {
                do_navigate(ev.screen_id);
                continue;
            }
            if (ev.type == UI_EVT_THEME_CHANGED) {
                if (s_active && s_active->apply_theme)
                    s_active->apply_theme();
                continue;
            }
            if (ev.type == UI_EVT_PROFILE_CHANGED) {
                do_rebuild_active();
                continue;
            }
            if (ev.type == UI_EVT_EVENT_FIRED) {
                // s_active_id always refers to the underlying screen (the
                // overlay model keeps it stable while the screensaver is up).
                if (s_active_id != SCREEN_EVENT_NOTIFICATION) {
                    screen_event_notification_set_return(s_active_id);
                }
                screen_event_notification_set_info(&ev.event_info);
                do_navigate(SCREEN_EVENT_NOTIFICATION);
                continue;
            }
            if (ev.type == UI_EVT_INPUT) {
                s_last_input_us = esp_timer_get_time();
                if (s_ss_overlay) {
                    // Any encoder/button action just dismisses the screensaver;
                    // the underlying screen does NOT receive the input.
                    dismiss_screensaver();
                    continue;
                }
                if (s_active && s_active->on_input)
                    s_active->on_input(ev.input);
                continue;
            }
            if (s_active && s_active->on_event)
                s_active->on_event(&ev);
        }

        // ── Idle-driven screensaver activation ─────────────────────────────
        if (!s_ss_overlay) {
            const app_state_t *st = app_state_get();
            if (st->scrsaver_delay > 0
                && can_auto_screensaver_from(s_active_id))
            {
                int64_t idle_s = (esp_timer_get_time() - s_last_input_us) / 1000000;
                if (idle_s >= st->scrsaver_delay) {
                    ESP_LOGI(TAG, "screensaver auto-activate (idle=%llds, id=%d)",
                             idle_s, st->scrsaver_id);
                    activate_screensaver(st->scrsaver_id);
                }
            }
        }

        uint32_t delay_ms = lv_timer_handler();
        if (delay_ms < 5)  delay_ms = 5;
        if (delay_ms > 50) delay_ms = 50;
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}