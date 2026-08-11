#include "screen_diag.h"
#include "ui_screen.h"
#include "ui_events.h"
#include "ui_manager.h"
#include "ui_profile.h"        // DISPLAY_WIDTH / DISPLAY_HEIGHT + settings_* metrics
#include "theme.h"
#include "diag.h"
#include "lvgl.h"
#include "esp_log.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "SCR_DIAG";

static lv_obj_t   *s_root   = NULL;
static lv_obj_t   *s_title  = NULL;
static lv_obj_t   *s_body   = NULL;   // scroll viewport
static lv_obj_t   *s_text   = NULL;   // the whole report, one wrapped label
static lv_obj_t   *s_hint   = NULL;
static lv_timer_t *s_timer  = NULL;

static diag_cpu_state_t s_cpu;        // this screen's own CPU baseline

/* ── formatting helpers ─────────────────────────────────────────────────────
   Integer-only: the montserrat_*_eu fonts and LVGL's own formatter make %f a
   liability, and KB/MB resolution is plenty for a diagnostics readout. */

static inline unsigned kb(size_t bytes) { return (unsigned)(bytes / 1024); }
static inline unsigned mb(uint64_t bytes) { return (unsigned)(bytes / (1024ULL * 1024ULL)); }

// Appends at `pos`, clamping at `cap` — so a truncated report degrades into a
// short one instead of running off the buffer. Returns the new position.
__attribute__((format(printf, 4, 5)))
static int append(char *buf, size_t cap, int pos, const char *fmt, ...)
{
    if (pos < 0 || (size_t)pos >= cap) return pos;
    va_list ap;
    va_start(ap, fmt);
    int w = vsnprintf(buf + pos, cap - pos, fmt, ap);
    va_end(ap);
    if (w < 0) return pos;
    pos += w;
    return (size_t)pos > cap ? (int)cap : pos;
}

static void fmt_uptime(char *buf, size_t n, uint32_t s)
{
    uint32_t d = s / 86400;
    uint32_t h = (s % 86400) / 3600;
    uint32_t m = (s % 3600) / 60;
    if (d > 0) snprintf(buf, n, "%ud %02u:%02u:%02u", (unsigned)d, (unsigned)h, (unsigned)m, (unsigned)(s % 60));
    else       snprintf(buf, n, "%02u:%02u:%02u", (unsigned)h, (unsigned)m, (unsigned)(s % 60));
}

/* ── report ─────────────────────────────────────────────────────────────────
   Rebuilt whole on every tick rather than kept as ~20 live labels: the text is
   under a kilobyte, and one label means one object to scroll and re-blend. */

static void refresh(void)
{
    if (!s_text) return;

    diag_info_t d;
    diag_collect(&d);
    int cpu0, cpu1;
    diag_cpu_usage(&s_cpu, &cpu0, &cpu1);

    char up[24];
    fmt_uptime(up, sizeof(up), d.uptime_s);

    char buf[960];
    int p = 0;
    p = append(buf, sizeof(buf), p, "FW    %s\n", d.fw_version);
    p = append(buf, sizeof(buf), p, "Build %s\n", d.fw_build);
    p = append(buf, sizeof(buf), p, "IDF   %s\n", d.idf_version);
    p = append(buf, sizeof(buf), p, "Var   %s\n", d.fw_variant ? d.fw_variant : "?");
    p = append(buf, sizeof(buf), p, "Web   %s\n", d.www_outdated ? "STALE" : "ok");
    if (d.update_available && d.update_latest[0])
        p = append(buf, sizeof(buf), p, "Upd   %s available\n", d.update_latest);

    // Spelled out rather than "min / blk": on the panel there is no room for a
    // legend, so each figure has to say what it is. "Lowest ever free" is the
    // low-water mark since boot, the number that matters when sizing anything
    // long-lived; "largest block" is what a TLS handshake (~6K) needs to find.
    p = append(buf, sizeof(buf), p, "\nPSRAM (external)\n");
    p = append(buf, sizeof(buf), p, "  %uK free of %uK\n", kb(d.psram_free), kb(d.psram_total));
    p = append(buf, sizeof(buf), p, "  %uK lowest ever free\n", kb(d.psram_min_free));
    p = append(buf, sizeof(buf), p, "Internal RAM\n");
    p = append(buf, sizeof(buf), p, "  %uK free of %uK\n", kb(d.int_free), kb(d.int_total));
    p = append(buf, sizeof(buf), p, "  %uK lowest ever free\n", kb(d.int_min_free));
    p = append(buf, sizeof(buf), p, "  %uK largest block\n", kb(d.int_largest));

    p = append(buf, sizeof(buf), p, "\nCPU   %d%% / %d%%\n", cpu0, cpu1);
    p = append(buf, sizeof(buf), p, "Up    %s\n", up);

    if (d.wifi_connected)
        p = append(buf, sizeof(buf), p, "\nWiFi  %s %d dBm\n", d.ssid, d.rssi);
    else
        p = append(buf, sizeof(buf), p, "\nWiFi  not connected\n");
    p = append(buf, sizeof(buf), p, "IP    %s\n", d.ip);
    p = append(buf, sizeof(buf), p, "MAC   %s\n", d.mac);

    p = append(buf, sizeof(buf), p, "\nChip  %s rev%d x%d\n", d.chip, d.chip_rev, d.chip_cores);
    // Die temperature, one decimal, sign handled by hand so -0.4 doesn't print
    // as "0.4". Plain "C", no "°" — the degree sign is available in the _eu
    // fonts now, but this readout is deliberately pure ASCII.
    if (d.temp_valid) {
        int t = d.temp_c10;
        p = append(buf, sizeof(buf), p, "Temp  %s%d.%d C\n",
                   t < 0 ? "-" : "", abs(t) / 10, abs(t) % 10);
    }
    p = append(buf, sizeof(buf), p, "Flash %uK\n", kb(d.flash_size));
    p = append(buf, sizeof(buf), p, "Panel %dx%d\n", DISPLAY_WIDTH, DISPLAY_HEIGHT);
    if (d.sd_mounted && d.sd_total)
        p = append(buf, sizeof(buf), p, "SD    %uM free of %uM", mb(d.sd_free), mb(d.sd_total));
    else if (d.sd_mounted)
        p = append(buf, sizeof(buf), p, "SD    mounted");
    else
        p = append(buf, sizeof(buf), p, "SD    not mounted");

    lv_label_set_text(s_text, buf);
}

static void timer_cb(lv_timer_t *t)
{
    (void)t;
    refresh();
}

/* ── lifecycle ──────────────────────────────────────────────────────────── */

static void diag_create(lv_obj_t *parent)
{
    s_root = parent;
    const ui_theme_colors_t *th = theme_get();
    const ui_profile_t      *p  = ui_profile_get();

    lv_obj_set_style_bg_color(parent, lv_color_hex(th->bg_primary), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, LV_PART_MAIN);

    int title_h = lv_font_get_line_height(p->settings_title_font);
    int hint_h  = lv_font_get_line_height(p->settings_hint_font);

    /* Panels short enough that Settings scrolls its own title (settings_title_in_list)
       have no vertical budget for a fixed header either — there the report starts
       at the very top and the "FW" line reads as the heading. */
    bool show_title = !p->settings_title_in_list;
    int  body_y     = show_title ? (p->settings_title_y + title_h + 2) : 0;
    int  body_h     = DISPLAY_HEIGHT + p->settings_hint_y - hint_h - body_y - 4;
    if (body_h < 16) body_h = 16;

    if (show_title) {
        s_title = lv_label_create(parent);
        lv_label_set_text(s_title, "Diagnostics");
        lv_obj_set_style_text_font(s_title, p->settings_title_font, LV_PART_MAIN);
        lv_obj_set_style_text_color(s_title, lv_color_hex(th->accent), LV_PART_MAIN);
        lv_obj_align(s_title, LV_ALIGN_TOP_MID, 0, p->settings_title_y);
    } else {
        s_title = NULL;
    }

    s_body = lv_obj_create(parent);
    lv_obj_set_size(s_body, p->settings_row_w + 12, body_h);
    lv_obj_align(s_body, LV_ALIGN_TOP_MID, 0, body_y);
    lv_obj_set_style_bg_opa(s_body, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_body, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_body, 2, LV_PART_MAIN);
    lv_obj_set_scroll_dir(s_body, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_body, LV_SCROLLBAR_MODE_AUTO);

    s_text = lv_label_create(s_body);
    lv_obj_set_width(s_text, p->settings_row_w + 4);
    lv_label_set_long_mode(s_text, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(s_text, p->settings_hint_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_text, lv_color_hex(th->text_primary), LV_PART_MAIN);
    lv_obj_align(s_text, LV_ALIGN_TOP_LEFT, 0, 0);

    s_hint = lv_label_create(parent);
    lv_label_set_text(s_hint, "Back: press or swipe");
    lv_obj_set_style_text_font(s_hint, p->settings_hint_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_hint, lv_color_hex(th->text_muted), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_hint, lv_color_hex(th->bg_primary), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_hint, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(s_hint, 4, LV_PART_MAIN);
    lv_obj_align(s_hint, LV_ALIGN_BOTTOM_MID, 0, p->settings_hint_y);

    memset(&s_cpu, 0, sizeof(s_cpu));   // fresh baseline: first tick reads 0/0
    refresh();
    s_timer = lv_timer_create(timer_cb, 1000, NULL);

    ESP_LOGI(TAG, "Created");
}

static void diag_destroy(void)
{
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    s_root = s_title = s_body = s_text = s_hint = NULL;
    ESP_LOGI(TAG, "Destroyed");
}

/* ── events & input ─────────────────────────────────────────────────────── */

static void diag_on_event(const ui_event_t *ev)
{
    (void)ev;   // everything on this screen comes from the 1 Hz poll
}

static void diag_on_input(ui_input_t input)
{
    int line = s_text ? lv_font_get_line_height(lv_obj_get_style_text_font(s_text, LV_PART_MAIN)) : 12;

    switch (input) {
        case UI_INPUT_ENCODER_CW:
            if (s_body) lv_obj_scroll_by(s_body, 0, -line, LV_ANIM_OFF);
            break;
        case UI_INPUT_ENCODER_CCW:
            if (s_body) lv_obj_scroll_by(s_body, 0, line, LV_ANIM_OFF);
            break;
        case UI_INPUT_ENCODER_PRESS:
        case UI_INPUT_ENCODER_LONG_PRESS:
        case UI_INPUT_SWIPE_LEFT:
        case UI_INPUT_SWIPE_RIGHT:
            ui_navigate(SCREEN_SETTINGS);
            break;
        default:
            break;
    }
}

static void diag_apply_theme(void)
{
    if (!s_root) return;
    const ui_theme_colors_t *th = theme_get();

    lv_obj_set_style_bg_color(s_root, lv_color_hex(th->bg_primary), LV_PART_MAIN);
    if (s_title) lv_obj_set_style_text_color(s_title, lv_color_hex(th->accent), LV_PART_MAIN);
    if (s_text)  lv_obj_set_style_text_color(s_text, lv_color_hex(th->text_primary), LV_PART_MAIN);
    if (s_hint) {
        lv_obj_set_style_text_color(s_hint, lv_color_hex(th->text_muted), LV_PART_MAIN);
        lv_obj_set_style_bg_color(s_hint, lv_color_hex(th->bg_primary), LV_PART_MAIN);
    }
    lv_obj_invalidate(s_root);
}

/* ── vtable ─────────────────────────────────────────────────────────────── */

const ui_screen_t screen_diag = {
    .create      = diag_create,
    .destroy     = diag_destroy,
    .apply_theme = diag_apply_theme,
    .on_event    = diag_on_event,
    .on_input    = diag_on_input,
    .name        = "diag",
};
