#include "ui_manager.h"
#include "ui_screen.h"
#include "ui_events.h"
#include "ui_background.h"
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
extern const ui_screen_t screen_mqtt;
extern const ui_screen_t screen_ota;
extern const ui_screen_t screen_sd_player;
extern const ui_screen_t screen_sd_browser;

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
    [SCREEN_MQTT]               = &screen_mqtt,
    [SCREEN_OTA]                = &screen_ota,
    [SCREEN_SD]                 = &screen_sd_player,
    [SCREEN_SD_BROWSER]         = &screen_sd_browser,
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
static bool           s_prev_bg_gradient = true;

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
    if (id == SCREEN_WIFI)               return false;
    if (id == SCREEN_OTA)                return false;  // never screensave mid-update
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

    if (s->bg_gradient != s_prev_bg_gradient) {
        s_prev_bg_gradient = s->bg_gradient;
        ui_event_t ev = { .type = UI_EVT_BG_CHANGED };
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

// Click-catcher on top of the screensaver: LV_EVENT_PRESSED doesn't bubble by
// default (unlike gestures), so a plain tap on screensaver widgets would never
// reach lv_scr_act(). The catcher posts UI_EVT_INPUT into the queue and the
// main loop dismisses the overlay safely on its next iteration.
static void ss_wake_cb(lv_event_t *e)
{
    (void)e;
    ui_input_send(UI_INPUT_NONE);
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

    lv_obj_t *catcher = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(catcher);
    lv_obj_set_size(catcher, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(catcher, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(catcher, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(catcher, ss_wake_cb, LV_EVENT_PRESSED, NULL);
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
    uie.event_info.hour     = e->hour;
    uie.event_info.minute   = e->minute;
    uie.event_info.is_alarm = (e->type == EV_ALARM);
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

// Pointer-indev press hook: LVGL touch widgets bypass the ui_manager event
// queue, so without this the idle timer would keep ticking while the user
// taps. Registered once per pointer indev at startup; fires before widget
// dispatch, so it works regardless of which widget catches the press.
static void pointer_press_note_cb(lv_event_t *e)
{
    (void)e;
    s_last_input_us = esp_timer_get_time();
}

// --------------------------------------------------------------------------
// Touch gesture dispatch — one screen-level handler, maps LVGL gesture
// direction to UI_INPUT_SWIPE_* and pushes through the normal input pipeline.
// LV_EVENT_GESTURE bubbles up from children by default (GESTURE_BUBBLE flag),
// so attaching to lv_scr_act() once catches swipes anywhere on the screen.
// --------------------------------------------------------------------------

static void gesture_dispatch_cb(lv_event_t *e)
{
    (void)e;
    lv_indev_t *indev = lv_indev_active();
    if (!indev) return;

    lv_dir_t dir = lv_indev_get_gesture_dir(indev);
    ui_input_t in;
    switch (dir) {
        case LV_DIR_LEFT:   in = UI_INPUT_SWIPE_LEFT;  break;
        case LV_DIR_RIGHT:  in = UI_INPUT_SWIPE_RIGHT; break;
        case LV_DIR_TOP:    in = UI_INPUT_SWIPE_UP;    break;
        case LV_DIR_BOTTOM: in = UI_INPUT_SWIPE_DOWN;  break;
        default:            return;
    }
    // Suppress the trailing click on the same press — without this,
    // controls_overlay (tap-to-show on screen_radio/bt) would pop up
    // every time a swipe ends.
    lv_indev_wait_release(indev);
    ui_input_send(in);
}

// --------------------------------------------------------------------------
// Main loop — the only place we touch LVGL
// --------------------------------------------------------------------------

void ui_manager_run(void)
{
    s_last_input_us = esp_timer_get_time();

    // Attach gesture handler to the (singleton) active screen object — it
    // survives lv_obj_clean() between navigations because clean only removes
    // children, not callbacks on the screen itself.
    // Clear scrollable flag so horizontal drags don't get swallowed as native
    // scroll (which would suppress LV_EVENT_GESTURE).
    lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(lv_scr_act(), gesture_dispatch_cb, LV_EVENT_GESTURE, NULL);

    // Dithered gradient background, applied once to the singleton screen object.
    // lv_obj_clean() only removes children, so it persists under every screen.
    ui_background_apply(lv_scr_act());

    for (lv_indev_t *id = lv_indev_get_next(NULL); id; id = lv_indev_get_next(id)) {
        if (lv_indev_get_type(id) == LV_INDEV_TYPE_POINTER) {
            lv_indev_add_event_cb(id, pointer_press_note_cb, LV_EVENT_PRESSED, NULL);
        }
    }

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
                ui_background_apply(lv_scr_act());   // swap gradient to match theme
                if (s_active && s_active->apply_theme)
                    s_active->apply_theme();
                continue;
            }
            if (ev.type == UI_EVT_BG_CHANGED) {
                ui_background_apply(lv_scr_act());   // gradient toggled on/off
                lv_obj_invalidate(lv_scr_act());
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