#include "defines.h"
#include "screen_sd_player.h"
#include "screen_sd_browser.h"
#include "ui_events.h"
#include "ui_screen.h"
#include "ui_manager.h"
#include "ui_nav.h"
#include "ui_label.h"
#include "controls_overlay_widget.h"
#include "touch_hotspots_widget.h"
#include "vol_overlay_widget.h"
#include "clock_widget.h"
#include "mode_indicator_widget.h"
#include "event_indicator_widget.h"
#include "weather_widget.h"
#include "vu_widget.h"
#include "vu_needle_widget.h"
#include "animated_wheels_widget.h"
#include "app_state.h"
#include "settings.h"
#include "sd_player.h"
#include "theme.h"
#include "ui_profile.h"
#include "fonts/ui_fonts.h"
#include "lvgl.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

// On-device SD music player ("now playing"). Mirrors screen_radio in layout —
// reuses the radio_* ui_profile fields — but shows the SD track/folder/modes and
// drives the SD player. The file browser lives in screen_sd_browser (opened with
// a long press / swipe-up, like radio → playlist).

static const char *TAG = "SCR_SD";

static lv_obj_t   *s_root   = NULL;
static lv_obj_t   *s_title  = NULL;   // track title (ID3 or file name)
static lv_obj_t   *s_folder = NULL;   // "<folder>   idx/count"
static lv_obj_t   *s_info   = NULL;   // "VOL: n%   PAUSED   SHUFFLE   REPEAT ..."
static lv_obj_t   *s_time   = NULL;   // "1:23 / 4:56" playback progress
static lv_obj_t   *s_bar    = NULL;   // read-only progress bar (hidden without a duration)
static lv_timer_t *s_tick   = NULL;   // 1 Hz refresh for the progress counter

// Last path segment for display ("/sdcard/music/Rock" → "Rock").
static const char *basename_of(const char *path)
{
    if (!path || !path[0]) return "";
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static void fmt_mmss(char *buf, size_t n, uint32_t ms)
{
    uint32_t s = ms / 1000;
    snprintf(buf, n, "%lu:%02lu", (unsigned long)(s / 60), (unsigned long)(s % 60));
}

// Refresh the "elapsed / total" counter. Driven by the 1 Hz timer (there is no
// per-second UI heartbeat) and once on create. Shows elapsed only until the
// length is known (or for formats without one).
static void bar_hide(void)
{
    if (s_bar) lv_obj_add_flag(s_bar, LV_OBJ_FLAG_HIDDEN);
}

static void progress_update(void)
{
    if (!s_time) return;
    if (!app_state_get()->sd_active) { ui_label_set_text(s_time, ""); bar_hide(); return; }

    uint32_t pos = sd_player_position_ms();
    uint32_t dur = sd_player_duration_ms();
    char p[8], d[8], out[20];
    fmt_mmss(p, sizeof(p), pos);
    if (dur) {
        if (pos > dur) pos = dur;
        fmt_mmss(d, sizeof(d), dur);
        snprintf(out, sizeof(out), "%s / %s", p, d);
    } else {
        snprintf(out, sizeof(out), "%s", p);
    }
    ui_label_set_text(s_time, out);

    // The bar needs a known length; formats without one (VBR w/o Xing, streams)
    // keep it hidden and show the elapsed counter alone.
    if (s_bar) {
        if (dur) {
            lv_bar_set_range(s_bar, 0, (int32_t)dur);
            lv_bar_set_value(s_bar, (int32_t)pos, LV_ANIM_OFF);
            lv_obj_clear_flag(s_bar, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_bar, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void tick_cb(lv_timer_t *t) { (void)t; progress_update(); }

static const char *repeat_str(int r)
{
    switch (r) {
        case 1:  return "REPEAT ALL";
        case 2:  return "REPEAT ONE";
        default: return "";
    }
}

static void refresh_from_state(void)
{
    if (!s_title) return;
    app_state_t *s = app_state_get();

    animated_wheels_widget_set_running(s->sd_active && !s->sd_paused);

    // Only show a track while SD is the active source — app_state.title is
    // shared and otherwise holds the radio's ICY title. Blank (hidden) when idle
    // so no empty plate shows; "Nothing playing" in the folder line covers it.
    const char *t = "";
    if (s->sd_active) {
        t = s->title[0] ? s->title : (s->sd_track[0] ? s->sd_track : "—");
    }
    ui_label_set_text(s_title, t);

    if (s_folder) {
        if (s->sd_active && s->sd_count > 0) {
            lv_label_set_text_fmt(s_folder, "%s   %d/%d",
                                  basename_of(s->sd_dir), s->sd_index + 1, s->sd_count);
        } else {
            lv_label_set_text(s_folder, s->sd_active ? basename_of(s->sd_dir) : "Nothing playing");
        }
    }

    if (s_info) {
        char info[96];
        int n = snprintf(info, sizeof(info), "VOL: %d%%", s->volume);
        if (s->sd_paused)  n += snprintf(info + n, sizeof(info) - n, "   PAUSED");
        if (s->sd_shuffle) n += snprintf(info + n, sizeof(info) - n, "   SHUFFLE");
        const char *r = repeat_str(s->sd_repeat);
        if (r[0])          n += snprintf(info + n, sizeof(info) - n, "   %s", r);
        lv_label_set_text(s_info, info);
    }

    progress_update();            // snap the counter on track/source change

    controls_overlay_refresh();   // keep center play/stop in sync with external changes
}

// Screen-centered, content-hugging label (so the label_bg plate tracks the
// text, not the full width) — mirrors the home clock labels. Long text is
// capped at the panel width (title uses LONG_DOT to ellipsize at the cap).
static lv_obj_t *make_centered_label(lv_obj_t *parent, const lv_font_t *font,
                                     uint32_t color, int16_t y)
{
    lv_obj_t *lbl = ui_anchored_label(parent, DISPLAY_WIDTH / 2, y, UI_ALIGN_CENTER);
    lv_label_set_text(lbl, "");
    lv_obj_set_style_max_width(lbl, DISPLAY_WIDTH - 20, LV_PART_MAIN);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, lv_color_hex(color), LV_PART_MAIN);
    ui_label_scrim(lbl);
    return lbl;
}

static void sd_player_screen_create(lv_obj_t *parent)
{
    s_root = parent;
    const ui_theme_colors_t *th = theme_get();
    const ui_profile_t      *p  = ui_profile_get();

    lv_obj_set_style_bg_color(parent, lv_color_hex(th->bg_primary), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, LV_PART_MAIN);

    if (p->sd_show_cassette &&
        (p->sd_show_wheel_left || p->sd_show_wheel_right)) {
        animated_wheels_widget_create(parent, (animated_wheels_style_t)p->sd_animation_style,
                                      p->sd_show_wheel_left,
                                      p->sd_cassette_l_x, p->sd_cassette_l_y, p->sd_cassette_l_size,
                                      p->sd_show_wheel_right,
                                      p->sd_cassette_r_x, p->sd_cassette_r_y, p->sd_cassette_r_size);
    }

    s_title = make_centered_label(parent, p->sd_title_font, th->text_primary, p->sd_title_y);

    if (p->sd_show_folder) {
        s_folder = make_centered_label(parent, p->sd_folder_font, th->accent,     p->sd_folder_y);
    }
    if (p->sd_show_info) {
        s_info   = make_centered_label(parent, p->sd_info_font,   th->text_muted, p->sd_info_y);
    }

    // The time row and bar sit below the VOL/mode line; anchor to it, falling
    // back to the next visible row above (folder → title) when info is hidden.
    #define SD_ROW_ANCHOR (s_info ? s_info : (s_folder ? s_folder : s_title))

    // Playback counter just below the VOL/mode line (position follows the
    // profile-driven anchor). Skipped on panels with no spare line (mono).
    if (p->sd_show_time) {
        // Fixed y under the anchor row (an anchored label re-pins its own y on
        // every text change, so align_to can't be used here — compute the y).
        int16_t anchor_y;  const lv_font_t *anchor_font;
        if (s_info)        { anchor_y = p->sd_info_y;   anchor_font = p->sd_info_font; }
        else if (s_folder) { anchor_y = p->sd_folder_y; anchor_font = p->sd_folder_font; }
        else               { anchor_y = p->sd_title_y;  anchor_font = p->sd_title_font; }
        int16_t time_y = anchor_y + lv_font_get_line_height(anchor_font) + 4;
        s_time = make_centered_label(parent, p->sd_info_font, th->text_muted, time_y);
        s_tick = lv_timer_create(tick_cb, 1000, NULL);
    }

    // Read-only progress bar under the time row. Anchored to s_time (or s_info
    // if the time row is off), sized from the profile. Kept hidden until a
    // duration is known (see progress_update).
    if (p->sd_show_bar && p->sd_bar_w > 0) {
        s_bar = lv_bar_create(parent);
        lv_obj_set_size(s_bar, p->sd_bar_w, p->sd_bar_h);
        lv_obj_set_style_bg_color(s_bar, lv_color_hex(th->text_muted), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(s_bar, LV_OPA_40, LV_PART_MAIN);
        lv_obj_set_style_bg_color(s_bar, lv_color_hex(th->accent), LV_PART_INDICATOR);
        lv_obj_set_style_radius(s_bar, p->sd_bar_h / 2, LV_PART_MAIN);
        lv_obj_set_style_radius(s_bar, p->sd_bar_h / 2, LV_PART_INDICATOR);
        lv_obj_align_to(s_bar, s_time ? s_time : SD_ROW_ANCHOR, LV_ALIGN_OUT_BOTTOM_MID, 0, 4);
        lv_obj_add_flag(s_bar, LV_OBJ_FLAG_HIDDEN);
    }
    #undef SD_ROW_ANCHOR

    // Clock + indicators (own sd_* layout fields, same widgets as screen_radio).
    if (p->sd_show_clock) {
        clock_widget_create(parent, p->sd_clock_widget_x, p->sd_clock_widget_y,
                            p->sd_clock_font, UI_ALIGN_CENTER);
    }
    if (p->sd_show_mode_indicator) {
        mode_indicator_create(parent, p->sd_mode_indic_x, p->sd_mode_indic_y);
    }
    if (p->sd_show_event_indicator) {
        event_indicator_create(parent, p->sd_event_indic_x, p->sd_event_indic_y);
    }
    if (p->sd_show_vu) {
        vu_widget_create(parent, p->sd_vu_x, p->sd_vu_y, p->sd_vu_w, p->sd_vu_h,
                         p->sd_vu_transparent);
    }
    if (p->sd_needle_show_l || p->sd_needle_show_r) {
        vu_needle_widget_create(parent,
                                p->sd_needle_show_l,
                                p->sd_needle_l_x, p->sd_needle_l_y,
                                p->sd_needle_l_w, p->sd_needle_l_h,
                                p->sd_needle_show_r,
                                p->sd_needle_r_x, p->sd_needle_r_y,
                                p->sd_needle_r_w, p->sd_needle_r_h,
                                p->sd_needle_frame);
    }
    if (p->sd_show_weather) {
        weather_widget_create(parent, p->sd_weather_x, p->sd_weather_y,
                              p->sd_weather_w, p->sd_weather_font);
    }

    refresh_from_state();
    progress_update();   // set/hide the time row now, before the 1 s tick

    if (p->sd_show_ctrl_overlay) {
        controls_overlay_create(parent, CTRL_OVL_MODE_SD);
    }
    touch_hotspots_widget_create(parent, CONTROL_SOURCE_SD, p->sd_touch_hotspots);

    ESP_LOGI(TAG, "Created");
}

static void sd_player_screen_destroy(void)
{
    if (s_tick) { lv_timer_delete(s_tick); s_tick = NULL; }
    touch_hotspots_widget_destroy();
    controls_overlay_destroy();
    vol_overlay_hide();
    vu_widget_destroy();
    vu_needle_widget_destroy();
    animated_wheels_widget_destroy();
    weather_widget_destroy();
    mode_indicator_destroy();
    event_indicator_destroy();
    clock_widget_destroy();
    s_root   = NULL;
    s_title  = NULL;
    s_folder = NULL;
    s_info   = NULL;
    s_time   = NULL;
    s_bar    = NULL;
    ESP_LOGI(TAG, "Destroyed");
}

static void sd_player_on_event(const ui_event_t *ev)
{
    switch (ev->type) {
        case UI_EVT_STATE_CHANGED:
            refresh_from_state();
            mode_indicator_update();
            event_indicator_update();
            clock_widget_tick();
            break;
        case UI_EVT_TITLE_CHANGED:
            refresh_from_state();
            break;
        case UI_EVT_WEATHER_UPDATE:
            weather_widget_update();
            break;
        default:
            break;
    }
}

static void sd_player_on_input(ui_input_t input)
{
    switch (input) {
        case UI_INPUT_ENCODER_CW:
        case UI_INPUT_ENCODER_CCW: {
            app_state_t *s = app_state_get();
            int vol = s->volume + ((input == UI_INPUT_ENCODER_CW) ? RADIO_VOL_STEP : -RADIO_VOL_STEP);
            if (vol < 0)   vol = 0;
            if (vol > 100) vol = 100;
            settings_set_volume(vol);
            vol_overlay_show(s_root, vol, true);
            break;
        }

        case UI_INPUT_ENCODER_PRESS:
        case UI_INPUT_SWIPE_RIGHT:
            ui_nav_ring_next(SCREEN_SD);
            break;
        case UI_INPUT_SWIPE_LEFT:
            ui_nav_ring_prev(SCREEN_SD);
            break;

        case UI_INPUT_ENCODER_LONG_PRESS:
        case UI_INPUT_SWIPE_UP:
            screen_sd_browser_set_return(SCREEN_SD);
            ui_navigate(SCREEN_SD_BROWSER);
            break;

        default:
            break;
    }
}

static void sd_player_apply_theme(void)
{
    if (!s_root) return;
    const ui_theme_colors_t *th = theme_get();

    lv_obj_set_style_bg_color(s_root, lv_color_hex(th->bg_primary), LV_PART_MAIN);
    if (s_title)  { lv_obj_set_style_text_color(s_title,  lv_color_hex(th->text_primary), LV_PART_MAIN); ui_label_scrim(s_title); }
    if (s_folder) { lv_obj_set_style_text_color(s_folder, lv_color_hex(th->accent),       LV_PART_MAIN); ui_label_scrim(s_folder); }
    if (s_info)   { lv_obj_set_style_text_color(s_info,   lv_color_hex(th->text_muted),   LV_PART_MAIN); ui_label_scrim(s_info); }
    if (s_time)   ui_label_scrim(s_time);
    if (s_bar) {
        lv_obj_set_style_bg_color(s_bar, lv_color_hex(th->text_muted), LV_PART_MAIN);
        lv_obj_set_style_bg_color(s_bar, lv_color_hex(th->accent),     LV_PART_INDICATOR);
    }

    clock_widget_apply_theme();
    mode_indicator_apply_theme();
    event_indicator_apply_theme();
    vu_widget_apply_theme();
    vu_needle_widget_apply_theme();
    animated_wheels_widget_apply_theme();

    lv_obj_invalidate(s_root);
}

const ui_screen_t screen_sd_player = {
    .create      = sd_player_screen_create,
    .destroy     = sd_player_screen_destroy,
    .apply_theme = sd_player_apply_theme,
    .on_event    = sd_player_on_event,
    .on_input    = sd_player_on_input,
    .name        = "sd_player",
};
