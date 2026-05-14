#include "screen_settings.h"
#include "app_state.h"
#include "settings.h"
#include "theme.h"
#include "ui_profile.h"
#include "fonts/ui_fonts.h"
#include "ui_screen.h"
#include "ui_events.h"
#include "ui_manager.h"
#include "lvgl.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "SCR_SETTINGS";

typedef enum {
    FOCUS_BRIGHTNESS = 0,
    FOCUS_THEME,
    FOCUS_MODE,
    FOCUS_BT_SCREEN,
    FOCUS_DSP,
    FOCUS_EQ,
    FOCUS_SS,
    FOCUS_EVENTS,
    FOCUS_COUNT
} settings_focus_t;

static lv_obj_t *s_root              = NULL;
static lv_obj_t *s_title             = NULL;
static lv_obj_t *s_rows_cont         = NULL;
static lv_obj_t *s_row_bright        = NULL;
static lv_obj_t *s_row_theme         = NULL;
static lv_obj_t *s_row_mode          = NULL;
static lv_obj_t *s_row_bt_screen     = NULL;
static lv_obj_t *s_row_dsp           = NULL;
static lv_obj_t *s_row_eq            = NULL;
static lv_obj_t *s_row_ss            = NULL;
static lv_obj_t *s_row_events        = NULL;
static lv_obj_t *s_slider_bright     = NULL;
static lv_obj_t *s_label_bright_val  = NULL;
static lv_obj_t *s_label_theme_val   = NULL;
static lv_obj_t *s_label_mode_val    = NULL;
static lv_obj_t *s_label_bt_screen_val = NULL;
static lv_obj_t *s_label_dsp_val     = NULL;
static lv_obj_t *s_label_eq_val      = NULL;
static lv_obj_t *s_label_ss_val      = NULL;
static lv_obj_t *s_label_events_val  = NULL;
static lv_obj_t *s_mem_bar           = NULL;
static lv_timer_t *s_mem_timer       = NULL;
static lv_obj_t *s_hint              = NULL;

static settings_focus_t s_focus = FOCUS_BRIGHTNESS;
static bool s_editing = false;

/* ── helpers ────────────────────────────────────────────────────────────── */

static bool focus_opens_screen(settings_focus_t f)
{
    return f == FOCUS_EQ || f == FOCUS_EVENTS;
}

static void update_focus_visuals(void)
{
    const ui_theme_colors_t *th = theme_get();
    lv_obj_t *rows[FOCUS_COUNT] = { 
        s_row_bright, s_row_theme, s_row_mode, s_row_bt_screen, s_row_dsp, s_row_eq, s_row_ss, s_row_events 
    };
    for (int i = 0; i < FOCUS_COUNT; i++) {
        if (!rows[i]) continue;
        bool focused = (i == (int)s_focus);
        uint32_t c = focused ? th->accent : th->text_muted;
        int w = (focused && s_editing) ? 4 : 2;
        lv_obj_set_style_border_color(rows[i], lv_color_hex(c), LV_PART_MAIN);
        lv_obj_set_style_border_width(rows[i], w, LV_PART_MAIN);
    }
    if (rows[s_focus]) {
        lv_obj_scroll_to_view(rows[s_focus], LV_ANIM_ON);
    }
}

static void update_hint(void)
{
    if (!s_hint) return;
    if (s_editing) {
        lv_label_set_text(s_hint, "turn=change  press=done");
    } else if (focus_opens_screen(s_focus)) {
        lv_label_set_text(s_hint, "turn=nav  press=open  long=back");
    } else {
        lv_label_set_text(s_hint, "turn=nav  press=edit  long=back");
    }
}

static void update_brightness_label(void)
{
    if (!s_label_bright_val || !s_slider_bright) return;
    int br = app_state_get()->brightness;
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", br);
    lv_label_set_text(s_label_bright_val, buf);
    lv_slider_set_value(s_slider_bright, br, LV_ANIM_OFF);
}

static void update_theme_label(void)
{
    if (!s_label_theme_val) return;
    lv_label_set_text(s_label_theme_val,
        theme_current() == THEME_LIGHT ? "<    Light    >" : "<    Dark    >");
}

static void update_mode_label(void)
{
    if (!s_label_mode_val) return;
    lv_label_set_text(s_label_mode_val,
        app_state_get()->bt_enable == true ? "<    Bluetooth    >" : "<    Radio    >");
}

static void update_bt_screen_label(void)
{
    if (!s_label_bt_screen_val) return;
    lv_label_set_text(s_label_bt_screen_val,
        app_state_get()->bt_show_screen == true ? "<    Show    >" : "<    Hide    >");
}

static void update_dsp_label(void)
{
    if (!s_label_dsp_val) return;
    lv_label_set_text(s_label_dsp_val,
        app_state_get()->eq_enabled == true ? "<    On    >" : "<    Off    >");
}

static void update_ss_label(void)
{
    if (!s_label_ss_val) return;
    int ss = app_state_get()->scrsaver_delay;
    char buf[16];
    if (ss == 0)
        snprintf(buf, sizeof(buf), "%s", "OFF");
    else
        snprintf(buf, sizeof(buf), "%ds", ss);
    lv_label_set_text(s_label_ss_val, buf);
}

/* ── CPU usage (per core, %) ────────────────────────────────────────────────
   Measures delta idle-task runtime per core relative to the last call.
   Requires CONFIG_FREERTOS_USE_TRACE_FACILITY + GENERATE_RUN_TIME_STATS in sdkconfig. */
static uint32_t s_last_idle_rt[2] = {0, 0};
static uint32_t s_last_total_rt   = 0;

static void compute_cpu_usage(int *cpu0, int *cpu1)
{
    *cpu0 = *cpu1 = 0;

    UBaseType_t n = uxTaskGetNumberOfTasks();
    if (n == 0) return;

    TaskStatus_t *array = pvPortMalloc(n * sizeof(TaskStatus_t));
    if (!array) return;

    uint32_t total_rt = 0;
    n = uxTaskGetSystemState(array, n, &total_rt);

    TaskHandle_t idle0 = xTaskGetIdleTaskHandleForCore(0);
    TaskHandle_t idle1 = xTaskGetIdleTaskHandleForCore(1);

    uint32_t idle_rt[2] = {0, 0};
    for (UBaseType_t i = 0; i < n; i++) {
        if      (array[i].xHandle == idle0) idle_rt[0] = array[i].ulRunTimeCounter;
        else if (array[i].xHandle == idle1) idle_rt[1] = array[i].ulRunTimeCounter;
    }
    vPortFree(array);

    // First call has no reference yet — return 0 and remember the values.
    if (s_last_total_rt != 0) {
        uint32_t total_delta = total_rt - s_last_total_rt;
        if (total_delta > 0) {
            uint32_t idle0_delta = idle_rt[0] - s_last_idle_rt[0];
            uint32_t idle1_delta = idle_rt[1] - s_last_idle_rt[1];
            int p0 = 100 - (int)((idle0_delta * 100) / total_delta);
            int p1 = 100 - (int)((idle1_delta * 100) / total_delta);
            if (p0 < 0)   p0 = 0;
            if (p0 > 100) p0 = 100;
            if (p1 < 0)   p1 = 0;
            if (p1 > 100) p1 = 100;
            *cpu0 = p0;
            *cpu1 = p1;
        }
    }

    s_last_total_rt   = total_rt;
    s_last_idle_rt[0] = idle_rt[0];
    s_last_idle_rt[1] = idle_rt[1];
}

static void update_mem_bar(void)
{
    if (!s_mem_bar) return;
    int free_total = (int)esp_get_free_heap_size();
    int free_int   = (int)heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    int free_blk   = (int)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    int cpu0, cpu1;
    compute_cpu_usage(&cpu0, &cpu1);

    // RSSI (dBm); "--" when STA not connected / no data
    char rssi_buf[8] = "--";
    wifi_ap_record_t ap;
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
        snprintf(rssi_buf, sizeof(rssi_buf), "%d", ap.rssi);
    }

    char buf[96];
    snprintf(buf, sizeof(buf), "Free:%dK Int:%dK Blk:%dK CPU:%d/%d%% RSSI:%s",
             free_total / 1024, free_int / 1024, free_blk / 1024,
             cpu0, cpu1, rssi_buf);
    lv_label_set_text(s_mem_bar, buf);
}

static void mem_timer_cb(lv_timer_t *t)
{
    (void)t;
    update_mem_bar();
}

/* ── row factory ────────────────────────────────────────────────────────── */

static lv_obj_t *make_row(lv_obj_t *parent, int y_ofs, const char *title)
{
    const ui_theme_colors_t *th = theme_get();
    const ui_profile_t      *p  = ui_profile_get();

    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, p->settings_row_w, p->settings_row_h);
    lv_obj_align(row, LV_ALIGN_TOP_MID, 0, y_ofs);
    lv_obj_set_style_bg_color(row, lv_color_hex(th->bg_secondary), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(row, lv_color_hex(th->text_muted), LV_PART_MAIN);
    lv_obj_set_style_radius(row, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_all(row, 10, LV_PART_MAIN);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(row);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_font(lbl, p->settings_row_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, lv_color_hex(th->text_secondary), LV_PART_MAIN);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 0, 0);

    return row;
}

/* ── lifecycle ──────────────────────────────────────────────────────────── */

static void settings_create(lv_obj_t *parent)
{
    s_root = parent;
    const ui_theme_colors_t *th = theme_get();
    const ui_profile_t      *p  = ui_profile_get();

    lv_obj_set_style_bg_color(parent, lv_color_hex(th->bg_primary), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, LV_PART_MAIN);

    /* title */
    s_title = lv_label_create(parent);
    lv_label_set_text(s_title, "Settings");
    lv_obj_set_style_text_font(s_title, p->settings_title_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_title, lv_color_hex(th->accent), LV_PART_MAIN);
    lv_obj_align(s_title, LV_ALIGN_TOP_MID, 0, p->settings_title_y);

    /* ── read-only memory bar (right below the title) ── */
    int mem_bar_h = lv_font_get_line_height(p->settings_hint_font);
    s_mem_bar = lv_label_create(parent);
    lv_obj_set_style_text_font(s_mem_bar, p->settings_hint_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_mem_bar, lv_color_hex(th->text_primary), LV_PART_MAIN);
    lv_obj_align(s_mem_bar, LV_ALIGN_TOP_MID, 0, p->settings_row1_y);
    update_mem_bar();

    /* ── scrollable rows container (between memory bar and hint) ── */
    int hint_h = lv_font_get_line_height(p->settings_hint_font);
    int rows_start_y = p->settings_row1_y + mem_bar_h + 2;
    int rows_cont_h = DISPLAY_HEIGHT + p->settings_hint_y - hint_h - rows_start_y - 4;
    s_rows_cont = lv_obj_create(parent);
    lv_obj_set_size(s_rows_cont, p->settings_row_w + 12, rows_cont_h);
    lv_obj_align(s_rows_cont, LV_ALIGN_TOP_MID, 0, rows_start_y);
    lv_obj_set_style_bg_opa(s_rows_cont, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_rows_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_rows_cont, 0, LV_PART_MAIN);
    lv_obj_set_scroll_dir(s_rows_cont, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_rows_cont, LV_SCROLLBAR_MODE_AUTO);

    /* ── row: Brightness ── */
    s_row_bright = make_row(s_rows_cont, 0, "Brightness");

    s_slider_bright = lv_slider_create(s_row_bright);
    lv_obj_set_size(s_slider_bright, p->settings_slider_w, p->settings_slider_h);
    lv_obj_align(s_slider_bright, LV_ALIGN_BOTTOM_LEFT, 0, -2);
    lv_slider_set_range(s_slider_bright, 10, 100);
    lv_obj_set_style_bg_color(s_slider_bright, lv_color_hex(th->accent),     LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_slider_bright, lv_color_hex(th->text_muted), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_slider_bright, LV_OPA_COVER, LV_PART_MAIN);
    /* slider is visual only — input is handled by on_input */
    lv_obj_clear_flag(s_slider_bright, LV_OBJ_FLAG_CLICKABLE);

    s_label_bright_val = lv_label_create(s_row_bright);
    lv_obj_set_style_text_font(s_label_bright_val, p->settings_value_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_label_bright_val, lv_color_hex(th->text_primary), LV_PART_MAIN);
    lv_obj_align(s_label_bright_val, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    /* ── row: Theme ── */
    s_row_theme = make_row(s_rows_cont, p->settings_row2_y - p->settings_row1_y, "Theme");

    s_label_theme_val = lv_label_create(s_row_theme);
    lv_obj_set_style_text_font(s_label_theme_val, p->settings_value_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_label_theme_val, lv_color_hex(th->text_primary), LV_PART_MAIN);
    lv_obj_align(s_label_theme_val, LV_ALIGN_BOTTOM_MID, 0, 0);

    /* ── row: Mode ── */
    s_row_mode = make_row(s_rows_cont, p->settings_row3_y - p->settings_row1_y, "Mode");

    s_label_mode_val = lv_label_create(s_row_mode);
    lv_obj_set_style_text_font(s_label_mode_val, p->settings_value_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_label_mode_val, lv_color_hex(th->text_primary), LV_PART_MAIN);
    lv_obj_align(s_label_mode_val, LV_ALIGN_BOTTOM_MID, 0, 0);

    /* ── row: BT Screen ── */
    int row4_y_ofs = (p->settings_row3_y - p->settings_row1_y)
                   + (p->settings_row2_y - p->settings_row1_y);
    s_row_bt_screen = make_row(s_rows_cont, row4_y_ofs, "BT Screen");

    s_label_bt_screen_val = lv_label_create(s_row_bt_screen);
    lv_obj_set_style_text_font(s_label_bt_screen_val, p->settings_value_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_label_bt_screen_val, lv_color_hex(th->text_primary), LV_PART_MAIN);
    lv_obj_align(s_label_bt_screen_val, LV_ALIGN_BOTTOM_MID, 0, 0);

    /* ── row: DSP (EQ on/off) ── */
    int row5_y_ofs = (p->settings_row3_y - p->settings_row1_y)
                   + 2 * (p->settings_row2_y - p->settings_row1_y);
    s_row_dsp = make_row(s_rows_cont, row5_y_ofs, "DSP");

    s_label_dsp_val = lv_label_create(s_row_dsp);
    lv_obj_set_style_text_font(s_label_dsp_val, p->settings_value_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_label_dsp_val, lv_color_hex(th->text_primary), LV_PART_MAIN);
    lv_obj_align(s_label_dsp_val, LV_ALIGN_BOTTOM_MID, 0, 0);

    /* ── row: Equalizer ── */
    int row6_y_ofs = (p->settings_row3_y - p->settings_row1_y)
                   + 3 * (p->settings_row2_y - p->settings_row1_y);
    s_row_eq = make_row(s_rows_cont, row6_y_ofs, "Equalizer");

    s_label_eq_val = lv_label_create(s_row_eq);
    lv_obj_set_style_text_font(s_label_eq_val, p->settings_value_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_label_eq_val, lv_color_hex(th->text_primary), LV_PART_MAIN);
    lv_label_set_text(s_label_eq_val, "Open >");
    lv_obj_align(s_label_eq_val, LV_ALIGN_BOTTOM_MID, 0, 0);

    /* ── row: Screensaver ── */
    int row7_y_ofs = (p->settings_row3_y - p->settings_row1_y)
                   + 4 * (p->settings_row2_y - p->settings_row1_y);
    s_row_ss = make_row(s_rows_cont, row7_y_ofs, "Screensaver");

    s_label_ss_val = lv_label_create(s_row_ss);
    lv_obj_set_style_text_font(s_label_ss_val, p->settings_value_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_label_ss_val, lv_color_hex(th->text_primary), LV_PART_MAIN);
    lv_obj_align(s_label_ss_val, LV_ALIGN_BOTTOM_MID, 0, 0);

    /* ── row: Events ── */
    int row8_y_ofs = (p->settings_row3_y - p->settings_row1_y)
                   + 5 * (p->settings_row2_y - p->settings_row1_y);
    s_row_events = make_row(s_rows_cont, row8_y_ofs, "Events");

    s_label_events_val = lv_label_create(s_row_events);
    lv_obj_set_style_text_font(s_label_events_val, p->settings_value_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_label_events_val, lv_color_hex(th->text_primary), LV_PART_MAIN);
    lv_label_set_text(s_label_events_val, "Open >");
    lv_obj_align(s_label_events_val, LV_ALIGN_BOTTOM_MID, 0, 0);

    /* hint */
    s_hint = lv_label_create(parent);
    lv_obj_set_style_text_font(s_hint, p->settings_hint_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_hint, lv_color_hex(th->text_muted), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_hint, lv_color_hex(th->bg_primary), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_hint, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(s_hint, 4, LV_PART_MAIN);
    lv_obj_align(s_hint, LV_ALIGN_BOTTOM_MID, 0, p->settings_hint_y);

    s_focus = FOCUS_BRIGHTNESS;
    s_editing = false;
    update_brightness_label();
    update_theme_label();
    update_mode_label();
    update_bt_screen_label();
    update_dsp_label();
    update_ss_label();
    update_focus_visuals();
    update_hint();

    s_mem_timer = lv_timer_create(mem_timer_cb, 1000, NULL);

    ESP_LOGI(TAG, "Created (theme=%d)", theme_current());
}

static void settings_destroy(void)
{
    if (s_mem_timer) { lv_timer_delete(s_mem_timer); s_mem_timer = NULL; }
    s_root = s_title = s_rows_cont = NULL;
    s_row_bright = s_row_theme = s_row_mode = s_row_bt_screen = s_row_dsp = s_row_eq = s_row_ss = s_row_events = NULL;
    s_slider_bright = s_label_bright_val = NULL;
    s_label_theme_val = s_label_mode_val = s_label_bt_screen_val = s_label_dsp_val = s_label_eq_val = s_label_events_val = NULL;
    s_mem_bar = s_hint = NULL;
    ESP_LOGI(TAG, "Destroyed");
}

/* ── events & input ─────────────────────────────────────────────────────── */

static void settings_on_event(const ui_event_t *ev)
{
    if (ev->type == UI_EVT_STATE_CHANGED) {
        update_brightness_label();
        update_theme_label();
        update_mode_label();
        update_bt_screen_label();
        update_dsp_label();
    }
}

static void settings_on_input(ui_input_t input)
{
    switch (input) {

        case UI_INPUT_ENCODER_CW:
        case UI_INPUT_ENCODER_CCW: {
            int delta = (input == UI_INPUT_ENCODER_CW) ? 1 : -1;

            if (s_editing) {
                if (s_focus == FOCUS_BRIGHTNESS) {
                    int br = app_state_get()->brightness + delta * 5;
                    if (br < 10)  br = 10;
                    if (br > 100) br = 100;
                    settings_set_brightness(br);
                    update_brightness_label();
                } else if (s_focus == FOCUS_THEME) {
                    /* binary — both directions toggle */
                    ui_theme_t next = (theme_current() == THEME_DARK) ? THEME_LIGHT : THEME_DARK;
                    settings_set_theme(next);
                    update_theme_label();
                } else if (s_focus == FOCUS_MODE) {
                    bool mode = !app_state_get()->bt_enable;
                    settings_set_bt_enable(mode);
                    update_mode_label();
                } else if (s_focus == FOCUS_BT_SCREEN) {
                    bool show = !app_state_get()->bt_show_screen;
                    settings_set_bt_show_screen(show);
                    update_bt_screen_label();
                } else if (s_focus == FOCUS_DSP) {
                    bool eq_en = !app_state_get()->eq_enabled;
                    settings_set_eq_enabled(eq_en);
                    update_dsp_label();
                }
                else if (s_focus == FOCUS_SS) {
                    int ss = app_state_get()->scrsaver_delay + delta * 5;
                    if (ss < 0) ss = 0;
                    if (ss > 600) ss = 600;
                    settings_set_scrsaver_delay(ss);
                    update_ss_label();
                }
            } else {
                int next = (int)s_focus + delta;
                if (next < 0) next = 0;
                if (next >= FOCUS_COUNT) next = FOCUS_COUNT - 1;
                if (next != (int)s_focus) {
                    s_focus = (settings_focus_t)next;
                    update_focus_visuals();
                    update_hint();
                }
            }
            break;
        }

        case UI_INPUT_ENCODER_PRESS:
            if (s_focus == FOCUS_EQ) {
                ui_navigate(SCREEN_EQ);
            } else if (s_focus == FOCUS_EVENTS) {
                ui_navigate(SCREEN_EVENTS);
            } else {
                s_editing = !s_editing;
                update_focus_visuals();
                update_hint();
            }
            break;

        case UI_INPUT_BTN_BACK:
        case UI_INPUT_ENCODER_LONG_PRESS:
            ui_navigate(SCREEN_CLOCK);
            break;

        default:
            break;
    }
}

/* ── theme ──────────────────────────────────────────────────────────────── */

static void settings_apply_theme(void)
{
    if (!s_root) return;
    const ui_theme_colors_t *th = theme_get();

    lv_obj_set_style_bg_color(s_root, lv_color_hex(th->bg_primary), LV_PART_MAIN);
    lv_obj_set_style_text_color(s_title, lv_color_hex(th->accent), LV_PART_MAIN);

    lv_obj_t *rows[FOCUS_COUNT] = { s_row_bright, s_row_theme, s_row_mode, s_row_bt_screen, s_row_dsp, s_row_eq, s_row_ss, s_row_events };
    for (int i = 0; i < FOCUS_COUNT; i++) {
        if (!rows[i]) continue;
        lv_obj_set_style_bg_color(rows[i], lv_color_hex(th->bg_secondary), LV_PART_MAIN);
        /* child[0] = title label added in make_row */
        lv_obj_t *lbl = lv_obj_get_child(rows[i], 0);
        if (lbl) lv_obj_set_style_text_color(lbl, lv_color_hex(th->text_secondary), LV_PART_MAIN);
    }

    lv_obj_set_style_bg_color(s_slider_bright, lv_color_hex(th->accent),    LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_slider_bright, lv_color_hex(th->text_muted), LV_PART_MAIN);
    lv_obj_set_style_text_color(s_label_bright_val, lv_color_hex(th->text_primary), LV_PART_MAIN);
    lv_obj_set_style_text_color(s_label_theme_val,  lv_color_hex(th->text_primary), LV_PART_MAIN);
    lv_obj_set_style_text_color(s_label_mode_val,   lv_color_hex(th->text_primary), LV_PART_MAIN);
    if (s_label_bt_screen_val) lv_obj_set_style_text_color(s_label_bt_screen_val, lv_color_hex(th->text_primary), LV_PART_MAIN);
    if (s_label_dsp_val)    lv_obj_set_style_text_color(s_label_dsp_val,    lv_color_hex(th->text_primary), LV_PART_MAIN);
    if (s_label_eq_val)     lv_obj_set_style_text_color(s_label_eq_val,     lv_color_hex(th->text_primary), LV_PART_MAIN);
    if (s_label_ss_val)     lv_obj_set_style_text_color(s_label_ss_val,     lv_color_hex(th->text_primary), LV_PART_MAIN);
    if (s_label_events_val) lv_obj_set_style_text_color(s_label_events_val, lv_color_hex(th->text_primary), LV_PART_MAIN);
    if (s_mem_bar)          lv_obj_set_style_text_color(s_mem_bar,          lv_color_hex(th->text_muted),   LV_PART_MAIN);
    lv_obj_set_style_text_color(s_hint,             lv_color_hex(th->text_muted),   LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_hint,               lv_color_hex(th->bg_primary),   LV_PART_MAIN);

    update_focus_visuals();   /* updates border accent/text_muted */
    update_theme_label();     /* refreshes text after theme change */
    update_mode_label();
    update_bt_screen_label();
    update_dsp_label();
    update_ss_label();
    lv_obj_invalidate(s_root);
}

/* ── vtable ─────────────────────────────────────────────────────────────── */

const ui_screen_t screen_settings = {
    .create      = settings_create,
    .destroy     = settings_destroy,
    .apply_theme = settings_apply_theme,
    .on_event    = settings_on_event,
    .on_input    = settings_on_input,
    .name        = "settings",
};