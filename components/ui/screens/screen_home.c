#include "defines.h"
#include "screen_home.h"
#include "mode_indicator_widget.h"
#include "event_indicator_widget.h"
#include "hub_overlay_widget.h"
#include "vol_overlay_widget.h"
#include "app_state.h"
#include "sd_player.h"
#include "settings.h"
#include "theme.h"
#include "ui_profile.h"
#include "fonts/ui_fonts.h"
#include "ui_screen.h"
#include "ui_label.h"
#include "ui_events.h"
#include "ui_manager.h"
#include "ui_nav.h"
#include "screen_settings.h"
#include "ntp_service.h"
#include "wifi_manager.h"
#include "mdns_service.h"
#include "lvgl.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

// The hub uses the clock_* ui_profile fields (it replaced the standalone clock
// screen, which the Home hub fully supersedes) and adds hub_overlay, which can
// reach all three sources plus the playlist / SD browser / settings.

static const char *TAG = "SCR_HOME";

static lv_obj_t  *s_root          = NULL;
static lv_obj_t  *s_strip         = NULL;
static lv_obj_t  *s_strip_station = NULL;
static lv_obj_t  *s_strip_title   = NULL;
static lv_obj_t  *s_time_label    = NULL;
static lv_obj_t  *s_date_label    = NULL;
static lv_obj_t  *s_netinfo_label = NULL;
static lv_timer_t *s_clock_timer  = NULL;

static void netinfo_update(void);

// Active audio source → which buttons the hub overlay drives. Priority: BT, then
// actively-playing radio, then SD if it's playing OR has a resumable queue
// (stop-keep "limbo"), else radio as the default.
static controls_overlay_mode_t home_ctrl_mode(void)
{
    app_state_t *s = app_state_get();
    if (s->bt_enable) return CTRL_OVL_MODE_BT;
    if (s->radio_state == RADIO_STATE_PLAYING ||
        s->radio_state == RADIO_STATE_BUFFERING) return CTRL_OVL_MODE_RADIO;
    if (s->sd_active || sd_player_has_queue()) return CTRL_OVL_MODE_SD;
    return CTRL_OVL_MODE_RADIO;
}

// ── helpers ─────────────────────────────────────────────────────────────────

static lv_obj_t *make_panel(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color)
{
    lv_obj_t *p = lv_obj_create(parent);
    lv_obj_set_size(p, w, h);
    lv_obj_set_pos(p, x, y);
    lv_obj_set_style_bg_color(p, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(p, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(p, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(p, 0, LV_PART_MAIN);
    lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    return p;
}

static bool clock_is_large_font(void)
{
    const ui_profile_t *p = ui_profile_get();
    if (!p->clock_time_font) return false;
    return lv_font_get_line_height(p->clock_time_font) >= 48;
}

static void update_clock_display(void)
{
    if (!s_time_label) return;

    if (!ntp_service_is_synced()) {
        lv_label_set_text(s_time_label, "00:00");
        if (s_date_label) lv_label_set_text(s_date_label, "Syncing...");
        return;
    }

    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);

    char time_buf[12];
    snprintf(time_buf, sizeof(time_buf), "%02d:%02d", t.tm_hour, t.tm_min);
    lv_label_set_text(s_time_label, time_buf);

    if (s_date_label) {
        char date_buf[32];
        static const char *days[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
        snprintf(date_buf, sizeof(date_buf), "%s  %04d-%02d-%02d",
                 days[t.tm_wday], t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
        lv_label_set_text(s_date_label, date_buf);
    }
}

static void clock_timer_cb(lv_timer_t *t) { (void)t; update_clock_display(); netinfo_update(); }

// ── strip ───────────────────────────────────────────────────────────────────

static void strip_update(void)
{
    if (!s_strip_station) return;
    app_state_t *s = app_state_get();

    const char *station, *title;
    if (s->bt_enable) {
        // BT: top line = artist (fallback "Bluetooth"), bottom = track title
        station = s->bt_artist[0] ? s->bt_artist : "Bluetooth";
        title   = s->bt_title;
    } else if (s->sd_active) {
        station = "SD Player";
        title   = s->title;
    } else {
        station = s->station_name[0] ? s->station_name : "Atlas Radio";
        title   = s->title;
    }
    lv_label_set_text(s_strip_station, station);
    lv_label_set_text(s_strip_title, title[0] ? title : "");
}

// ── network info (IP + "<hostname>.local") ──────────────────────────────────

static void netinfo_update(void)
{
    const ui_profile_t *p = ui_profile_get();
    if (!s_root) return;

    if (!p->clock_show_netinfo) {
        if (s_netinfo_label) { lv_obj_del(s_netinfo_label); s_netinfo_label = NULL; }
        return;
    }

    if (!s_netinfo_label) {
        const ui_theme_colors_t *th = theme_get();
        s_netinfo_label = ui_anchored_label(s_root, p->clock_netinfo_x,
                                            p->clock_netinfo_y, UI_ALIGN_CENTER);
        lv_obj_set_style_text_font(s_netinfo_label,
            p->clock_netinfo_font ? p->clock_netinfo_font : &lv_font_montserrat_12_pl,
            LV_PART_MAIN);
        lv_obj_set_style_text_color(s_netinfo_label,
            lv_color_hex(th->text_muted), LV_PART_MAIN);
    }

    char ip[16];
    char host[40];
    wifi_get_ip(ip, sizeof(ip));
    mdns_effective_hostname(host, sizeof(host));

    char buf[64];
    snprintf(buf, sizeof(buf), "%s   %s.local", ip, host);
    lv_label_set_text(s_netinfo_label, buf);
}

// ── create / destroy ────────────────────────────────────────────────────────

static void home_create(lv_obj_t *parent)
{
    s_root = parent;
    const ui_theme_colors_t *th = theme_get();
    const ui_profile_t      *p  = ui_profile_get();

    if (p->clock_show_time) {
        s_time_label = ui_anchored_label(parent, p->clock_time_x, p->clock_time_y,
                                         UI_ALIGN_CENTER);
        lv_label_set_text(s_time_label, "--:--");
        lv_obj_set_style_text_font(s_time_label,
            p->clock_time_font ? p->clock_time_font : &lv_font_montserrat_18_pl,
            LV_PART_MAIN);
        lv_obj_set_style_text_color(s_time_label,
            lv_color_hex(th->text_primary), LV_PART_MAIN);
    }

    if (p->clock_show_date) {
        s_date_label = ui_anchored_label(parent, p->clock_date_x, p->clock_date_y,
                                         UI_ALIGN_CENTER);
        lv_label_set_text(s_date_label, "");
        lv_obj_set_style_text_font(s_date_label,
            p->clock_date_font ? p->clock_date_font : &lv_font_montserrat_18_pl,
            LV_PART_MAIN);
        lv_obj_set_style_text_color(s_date_label,
            lv_color_hex(th->text_secondary), LV_PART_MAIN);
    }

    s_clock_timer = lv_timer_create(clock_timer_cb, 60 * 1000, NULL);
    update_clock_display();

    if (p->clock_show_mode_indicator) {
        mode_indicator_create(parent, p->clock_mode_indic_x, p->clock_mode_indic_y);
    }

    if (p->clock_show_event_indicator) {
        event_indicator_create(parent, p->clock_event_indic_x, p->clock_event_indic_y);
    }

    if (p->clock_show_strip) {
        s_strip = make_panel(parent, p->clock_strip_x, p->clock_strip_y,
                             p->clock_strip_w, p->clock_strip_h, th->bg_secondary);

        s_strip_station = lv_label_create(s_strip);
        lv_label_set_long_mode(s_strip_station, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_width(s_strip_station, p->clock_strip_label_w);
        lv_obj_set_style_text_font(s_strip_station, p->clock_strip_station_font, LV_PART_MAIN);
        lv_obj_set_style_text_color(s_strip_station,
            lv_color_hex(th->text_secondary), LV_PART_MAIN);
        lv_obj_align(s_strip_station, LV_ALIGN_TOP_MID, 0, p->clock_strip_station_y);

        s_strip_title = lv_label_create(s_strip);
        lv_label_set_long_mode(s_strip_title, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_width(s_strip_title, p->clock_strip_label_w);
        lv_obj_set_style_text_font(s_strip_title, p->clock_strip_title_font, LV_PART_MAIN);
        lv_obj_set_style_text_color(s_strip_title,
            lv_color_hex(th->text_muted), LV_PART_MAIN);
        lv_obj_align(s_strip_title, LV_ALIGN_TOP_MID, 0, p->clock_strip_title_y);

        strip_update();
    }

    netinfo_update();

    hub_overlay_create(parent, home_ctrl_mode());

    ESP_LOGI(TAG, "Created (theme=%d)", theme_current());
}

static void home_destroy(void)
{
    if (s_clock_timer) { lv_timer_del(s_clock_timer); s_clock_timer = NULL; }
    hub_overlay_destroy();
    vol_overlay_hide();

    event_indicator_destroy();
    mode_indicator_destroy();
    s_root = s_strip = NULL;
    s_strip_station = s_strip_title = NULL;
    s_time_label    = s_date_label  = NULL;
    s_netinfo_label = NULL;

    ESP_LOGI(TAG, "Destroyed");
}

static void home_on_event(const ui_event_t *ev)
{
    switch (ev->type) {
        case UI_EVT_STATE_CHANGED:
            strip_update();
            update_clock_display();
            netinfo_update();
            mode_indicator_update();
            event_indicator_update();
            hub_overlay_set_mode(home_ctrl_mode());
            break;
        case UI_EVT_TITLE_CHANGED:
            strip_update();
            break;
        default:
            break;
    }
}

static void home_on_input(ui_input_t input)
{
    switch (input) {
        case UI_INPUT_ENCODER_CW:
        case UI_INPUT_ENCODER_CCW: {
            app_state_t *s = app_state_get();
            if (s->bt_enable != true) {
                int vol = s->volume;
                vol += (input == UI_INPUT_ENCODER_CW) ? RADIO_VOL_STEP : -RADIO_VOL_STEP;
                if (vol < 0)   vol = 0;
                if (vol > 100) vol = 100;
                settings_set_volume(vol);
                vol_overlay_show(s_root, vol, clock_is_large_font());
            } else {
                int vol = s->bt_volume;
                vol += (input == UI_INPUT_ENCODER_CW) ? BT_VOL_STEP : -BT_VOL_STEP;
                if (vol < 0)   vol = 0;
                if (vol > 100) vol = 100;
                settings_set_bt_volume(vol);
                vol_overlay_show(s_root, vol, clock_is_large_font());
            }
            break;
        }
        case UI_INPUT_ENCODER_PRESS:
        case UI_INPUT_SWIPE_RIGHT:
            ui_nav_ring_next(SCREEN_HOME);
            break;
        case UI_INPUT_SWIPE_LEFT:
            ui_nav_ring_prev(SCREEN_HOME);
            break;
        case UI_INPUT_ENCODER_LONG_PRESS:
        case UI_INPUT_SWIPE_UP:
            screen_settings_set_return(SCREEN_HOME);
            ui_navigate(SCREEN_SETTINGS);
            break;
        default:
            break;
    }
}

static void home_apply_theme(void)
{
    if (!s_root) return;
    const ui_theme_colors_t *th = theme_get();

    if (s_time_label)
        lv_obj_set_style_text_color(s_time_label,
            lv_color_hex(th->text_primary), LV_PART_MAIN);
    if (s_date_label)
        lv_obj_set_style_text_color(s_date_label,
            lv_color_hex(th->text_secondary), LV_PART_MAIN);
    if (s_netinfo_label)
        lv_obj_set_style_text_color(s_netinfo_label,
            lv_color_hex(th->text_muted), LV_PART_MAIN);

    if (s_strip) {
        lv_obj_set_style_bg_color(s_strip, lv_color_hex(th->bg_secondary), LV_PART_MAIN);
        lv_obj_set_style_text_color(s_strip_station,
            lv_color_hex(th->text_secondary), LV_PART_MAIN);
        lv_obj_set_style_text_color(s_strip_title,
            lv_color_hex(th->text_muted), LV_PART_MAIN);
    }

    mode_indicator_apply_theme();
    event_indicator_apply_theme();
    lv_obj_invalidate(s_root);
}

const ui_screen_t screen_home = {
    .create         = home_create,
    .destroy        = home_destroy,
    .apply_theme    = home_apply_theme,
    .on_event       = home_on_event,
    .on_input       = home_on_input,
    .name           = "home",
};
