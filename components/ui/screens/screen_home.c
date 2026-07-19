#include "defines.h"
#include "screen_home.h"
#include "mode_indicator_widget.h"
#include "event_indicator_widget.h"
#include "calendar_widget.h"
#include "weather_widget.h"
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
#include "ntp_service.h"
#include "ui_timefmt.h"
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
static lv_obj_t  *s_ampm_label    = NULL;
static int32_t    s_ampm_y_ofs    = 0;
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

static void home_time_scrim(lv_obj_t *label)
{
    const ui_profile_t *p = ui_profile_get();
    ui_label_scrim(label, p->clock_label_bg_opa);

    if (!label || !clock_is_large_font() || p->clock_label_bg_opa <= 0) return;

    // Large digit-only fonts leave slightly more unused line-box space above
    // the glyphs than below. Remove 2 px from the plate's top while keeping the
    // digits and the bottom edge in exactly the same visual position.
    lv_obj_set_style_pad_top(label, -1, LV_PART_MAIN);
    lv_obj_set_style_translate_y(label, 2, LV_PART_MAIN);
}

static void update_clock_display(void)
{
    if (!s_time_label) return;

    if (!ntp_service_is_synced()) {
        lv_label_set_text(s_time_label, "00:00");
        if (s_ampm_label) lv_obj_add_flag(s_ampm_label, LV_OBJ_FLAG_HIDDEN);
        if (s_date_label) lv_label_set_text(s_date_label, "Syncing...");
        return;
    }

    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);

    char time_buf[12];
    const char *suffix = ui_format_time(time_buf, sizeof(time_buf), &t);
    lv_label_set_text(s_time_label, time_buf);

    if (s_ampm_label) {
        if (suffix[0]) {
            lv_label_set_text(s_ampm_label, suffix);
            lv_obj_clear_flag(s_ampm_label, LV_OBJ_FLAG_HIDDEN);
            // Re-anchor on every tick: align_to is one-shot and the time
            // label's width (and centred position) changes with its text.
            lv_obj_update_layout(s_time_label);
            lv_obj_align_to(s_ampm_label, s_time_label,
                            LV_ALIGN_OUT_RIGHT_BOTTOM, 6, s_ampm_y_ofs);
        } else {
            lv_obj_add_flag(s_ampm_label, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (s_date_label) {
        char date_buf[32], date_part[16];
        static const char *days[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
        ui_format_date(date_part, sizeof(date_part), &t);
        snprintf(date_buf, sizeof(date_buf), "%s  %s", days[t.tm_wday], date_part);
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
    ui_label_set_text(s_strip_title, title);
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
        ui_label_scrim(s_netinfo_label, p->clock_label_bg_opa);
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
        home_time_scrim(s_time_label);

        // AM/PM suffix lives in its own label: the large clock fonts are
        // digit-only, so letters must use a text-capable (date) font. Hidden
        // in 24h mode; positioned relative to the time label on every update.
        const lv_font_t *tf = p->clock_time_font ? p->clock_time_font
                                                 : &lv_font_montserrat_18_pl;
        const lv_font_t *af = p->clock_date_font ? p->clock_date_font
                                                 : &lv_font_montserrat_18_pl;
        s_ampm_label = lv_label_create(parent);
        lv_label_set_text(s_ampm_label, "");
        lv_obj_add_flag(s_ampm_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_font(s_ampm_label, af, LV_PART_MAIN);
        lv_obj_set_style_text_color(s_ampm_label,
            lv_color_hex(th->text_secondary), LV_PART_MAIN);
        ui_label_scrim(s_ampm_label, p->clock_label_bg_opa);
        // Align the two baselines: OUT_RIGHT_BOTTOM matches the line-box
        // bottoms, so shift by the difference of the fonts' baseline offsets.
        s_ampm_y_ofs = (int32_t)af->base_line - (int32_t)tf->base_line;
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
        ui_label_scrim(s_date_label, p->clock_label_bg_opa);
    }

    s_clock_timer = lv_timer_create(clock_timer_cb, 60 * 1000, NULL);
    update_clock_display();

    if (p->clock_show_mode_indicator) {
        mode_indicator_create(parent, p->clock_mode_indic_x, p->clock_mode_indic_y);
    }

    if (p->clock_show_event_indicator) {
        event_indicator_create(parent, p->clock_event_indic_x, p->clock_event_indic_y);
    }

    if (p->clock_show_calendar) {
        calendar_widget_create(parent, p->clock_calendar_x, p->clock_calendar_y,
                               p->clock_calendar_w, p->clock_calendar_font);
    }
    if (p->clock_show_weather) {
        weather_widget_create(parent, p->clock_weather_x, p->clock_weather_y,
                              p->clock_weather_w, p->clock_weather_font,
                              p->clock_label_bg_opa);
    }

    // The strip container also anchors the station/title labels, so it must
    // always exist. clock_show_strip controls only its background plate.
    s_strip = make_panel(parent, p->clock_strip_x, p->clock_strip_y,
                         p->clock_strip_w, p->clock_strip_h, th->bg_secondary);
    int strip_opa = p->clock_show_strip ? p->clock_strip_bg_opa : 0;
    if (strip_opa < 0) strip_opa = 0;
    if (strip_opa > 100) strip_opa = 100;
    lv_obj_set_style_bg_opa(s_strip, (strip_opa * 255) / 100, LV_PART_MAIN);

    s_strip_station = lv_label_create(s_strip);
    lv_label_set_long_mode(s_strip_station, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(s_strip_station, p->clock_strip_label_w);
    lv_obj_set_style_text_font(s_strip_station, p->clock_strip_station_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_strip_station,
        lv_color_hex(th->text_secondary), LV_PART_MAIN);
    ui_label_scrim(s_strip_station, p->clock_label_bg_opa);
    lv_obj_align(s_strip_station, LV_ALIGN_TOP_MID, 0, p->clock_strip_station_y);

    s_strip_title = lv_label_create(s_strip);
    lv_label_set_long_mode(s_strip_title, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(s_strip_title, p->clock_strip_label_w);
    lv_obj_set_style_text_font(s_strip_title, p->clock_strip_title_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_strip_title,
        lv_color_hex(th->text_muted), LV_PART_MAIN);
    ui_label_scrim(s_strip_title, p->clock_label_bg_opa);
    lv_obj_align(s_strip_title, LV_ALIGN_TOP_MID, 0, p->clock_strip_title_y);

    strip_update();

    netinfo_update();

    hub_overlay_create(parent, home_ctrl_mode());

    ESP_LOGI(TAG, "Created (theme=%d)", theme_current());
}

static void home_destroy(void)
{
    if (s_clock_timer) { lv_timer_del(s_clock_timer); s_clock_timer = NULL; }
    hub_overlay_destroy();
    vol_overlay_hide();

    calendar_widget_destroy();
    weather_widget_destroy();
    event_indicator_destroy();
    mode_indicator_destroy();
    s_root = s_strip = NULL;
    s_strip_station = s_strip_title = NULL;
    s_time_label    = s_date_label  = NULL;
    s_ampm_label    = NULL;
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
            calendar_widget_update();
            weather_widget_update();
            hub_overlay_set_mode(home_ctrl_mode());
            break;
        case UI_EVT_TITLE_CHANGED:
            strip_update();
            break;
        case UI_EVT_WEATHER_UPDATE:
            weather_widget_update();
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
            // While the hub overlay is up, the encoder walks the buttons instead
            // of changing volume (volume is still reachable via the vol +/- ones).
            if (hub_overlay_is_visible()) {
                (input == UI_INPUT_ENCODER_CW) ? hub_overlay_focus_next()
                                               : hub_overlay_focus_prev();
                break;
            }
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
            // With the overlay open, a press fires the focused button; otherwise
            // it advances the screen ring as before.
            if (hub_overlay_is_visible()) {
                hub_overlay_activate();
                break;
            }
            ui_nav_ring_next(SCREEN_HOME);
            break;
        case UI_INPUT_SWIPE_RIGHT:
            ui_nav_ring_next(SCREEN_HOME);
            break;
        case UI_INPUT_SWIPE_LEFT:
            ui_nav_ring_prev(SCREEN_HOME);
            break;
        case UI_INPUT_ENCODER_LONG_PRESS:
            hub_overlay_show();
            break;
        default:
            break;
    }
}

static void home_apply_theme(void)
{
    if (!s_root) return;
    const ui_theme_colors_t *th = theme_get();
    const ui_profile_t      *p  = ui_profile_get();

    if (s_time_label) {
        lv_obj_set_style_text_color(s_time_label,
            lv_color_hex(th->text_primary), LV_PART_MAIN);
        home_time_scrim(s_time_label);
    }
    if (s_ampm_label) {
        lv_obj_set_style_text_color(s_ampm_label,
            lv_color_hex(th->text_secondary), LV_PART_MAIN);
        ui_label_scrim(s_ampm_label, p->clock_label_bg_opa);
    }
    if (s_date_label) {
        lv_obj_set_style_text_color(s_date_label,
            lv_color_hex(th->text_secondary), LV_PART_MAIN);
        ui_label_scrim(s_date_label, p->clock_label_bg_opa);
    }
    if (s_netinfo_label) {
        lv_obj_set_style_text_color(s_netinfo_label,
            lv_color_hex(th->text_muted), LV_PART_MAIN);
        ui_label_scrim(s_netinfo_label, p->clock_label_bg_opa);
    }

    if (s_strip) {
        lv_obj_set_style_bg_color(s_strip, lv_color_hex(th->bg_secondary), LV_PART_MAIN);
        lv_obj_set_style_text_color(s_strip_station,
            lv_color_hex(th->text_secondary), LV_PART_MAIN);
        lv_obj_set_style_text_color(s_strip_title,
            lv_color_hex(th->text_muted), LV_PART_MAIN);
        ui_label_scrim(s_strip_station, p->clock_label_bg_opa);
        ui_label_scrim(s_strip_title, p->clock_label_bg_opa);
    }

    mode_indicator_apply_theme();
    event_indicator_apply_theme();
    calendar_widget_apply_theme();
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
