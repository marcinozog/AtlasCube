#include "screen_settings.h"
#include "sdkconfig.h"   // CONFIG_TOUCH_NONE gates the WiFi menu entry
#include "app_state.h"
#include "settings.h"
#include "theme.h"
#include "ui_profile.h"
#include "fonts/ui_fonts.h"
#include "ui_screen.h"
#include "ui_events.h"
#include "ui_manager.h"
#include "screen_layout_editor.h"
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

static ui_screen_id_t s_return = SCREEN_HOME;   // where to go when settings is exited

void screen_settings_set_return(ui_screen_id_t scr) { s_return = scr; }

/* ── data-driven model ──────────────────────────────────────────────────────
   The screen is two levels deep: a top-level MENU of categories, each entry
   opening a sub-section with its own short list of rows. Both the menu and the
   sections are described by the same row-descriptor table, so adding an option
   is a single entry — no new focus enum, no new if/else branch. */

typedef enum {
    SECTION_MENU = 0,   // category list (Display / Audio / …)
    SECTION_DISPLAY,
    SECTION_AUDIO,
    SECTION_SCREENS,
    SECTION_SYSTEM,
    SECTION_COUNT
} settings_section_t;

typedef enum {
    RK_TOGGLE,    // press flips a bool; label shows on/off text
    RK_SLIDER,    // edit mode + encoder (and an optional touch slider) change a value
    RK_SCREEN,    // press navigates to another UI screen
    RK_SECTION,   // press enters a sub-section
    RK_ACTION,    // press fires a one-shot action
} row_kind_t;

typedef struct {
    const char    *title;
    row_kind_t     kind;
    /* RK_TOGGLE */
    bool          (*tget)(void);
    void          (*tset)(bool);
    const char    *on_txt;
    const char    *off_txt;
    /* RK_SLIDER */
    int           (*sget)(void);
    void          (*sset)(int);
    int            vmin, vmax, vstep;
    void          (*sfmt)(char *buf, size_t n, int v);
    /* RK_SCREEN / RK_SECTION / RK_ACTION */
    ui_screen_id_t      screen;
    settings_section_t  section;
    void               (*action)(void);
} row_desc_t;

#define MAX_ROWS 8
#define N_ROWS(a) ((int)(sizeof(a) / sizeof((a)[0])))

/* ── widgets ────────────────────────────────────────────────────────────── */

static lv_obj_t *s_root      = NULL;
static lv_obj_t *s_title     = NULL;
static lv_obj_t *s_rows_cont = NULL;
static lv_obj_t *s_hint      = NULL;
static lv_obj_t *s_mem_bar   = NULL;   // only built in SECTION_SYSTEM
static lv_timer_t *s_mem_timer = NULL;

static lv_obj_t *s_row_obj[MAX_ROWS];
static lv_obj_t *s_row_val[MAX_ROWS];
static lv_obj_t *s_row_slider[MAX_ROWS];

/* ── navigation state ───────────────────────────────────────────────────── */

static settings_section_t s_section = SECTION_MENU;
static const row_desc_t  *s_rows    = NULL;   // rows of the current section
static int                s_count   = 0;
static int                s_focus   = 0;
static bool               s_editing = false;  // slider edit mode

/* ── value formatters ───────────────────────────────────────────────────── */

static void fmt_pct(char *b, size_t n, int v)  { snprintf(b, n, "%d%%", v); }
static void fmt_secs(char *b, size_t n, int v) { if (v == 0) snprintf(b, n, "OFF"); else snprintf(b, n, "%ds", v); }

/* ── getters / setters (field access can't be taken as a function pointer) ── */

static int  get_brightness(void) { return app_state_get()->brightness; }
static int  get_scrsaver(void)   { return app_state_get()->scrsaver_delay; }
static bool get_theme_light(void){ return theme_current() == THEME_LIGHT; }
static void set_theme_light(bool v) { settings_set_theme(v ? THEME_LIGHT : THEME_DARK); }
static bool get_bt_enable(void)  { return app_state_get()->bt_enable; }
static bool get_bt_show(void)    { return app_state_get()->bt_show_screen; }
static bool get_sd_show(void)    { return app_state_get()->sd_show_screen; }
static bool get_radio_show(void) { return app_state_get()->radio_show_screen; }
static bool get_bt_auto(void)    { return app_state_get()->bt_auto_switch; }
static bool get_eq(void)         { return app_state_get()->eq_enabled; }
static void act_restart(void)    { esp_restart(); }
#if !CONFIG_TOUCH_NONE
static void act_radio_layout(void) { screen_layout_editor_open(LAYOUT_EDITOR_RADIO); }
#endif

/* ── row tables ─────────────────────────────────────────────────────────── */

static const row_desc_t SEC_MENU[] = {
    { .title = "Display", .kind = RK_SECTION, .section = SECTION_DISPLAY },
    { .title = "Audio",   .kind = RK_SECTION, .section = SECTION_AUDIO   },
    { .title = "Screens", .kind = RK_SECTION, .section = SECTION_SCREENS },
    { .title = "Events",  .kind = RK_SCREEN,  .screen  = SCREEN_EVENTS   },
    { .title = "System",  .kind = RK_SECTION, .section = SECTION_SYSTEM  },
};

static const row_desc_t SEC_DISPLAY[] = {
    { .title = "Brightness",  .kind = RK_SLIDER, .sget = get_brightness, .sset = settings_set_brightness,
      .vmin = 10, .vmax = 100, .vstep = 5, .sfmt = fmt_pct },
    { .title = "Theme",       .kind = RK_TOGGLE, .tget = get_theme_light, .tset = set_theme_light,
      .on_txt = "<    Light    >", .off_txt = "<    Dark    >" },
    { .title = "Screensaver", .kind = RK_SLIDER, .sget = get_scrsaver, .sset = settings_set_scrsaver_delay,
      .vmin = 0, .vmax = 600, .vstep = 5, .sfmt = fmt_secs },
};

static const row_desc_t SEC_AUDIO[] = {
    { .title = "Mode", .kind = RK_TOGGLE, .tget = get_bt_enable, .tset = settings_set_bt_enable,
      .on_txt = "<    Bluetooth    >", .off_txt = "<    Radio    >" },
    { .title = "Auto-switch audio source", .kind = RK_TOGGLE, .tget = get_bt_auto, .tset = settings_set_bt_auto_switch,
      .on_txt = "<    On    >", .off_txt = "<    Off    >" },
    { .title = "DSP", .kind = RK_TOGGLE, .tget = get_eq, .tset = settings_set_eq_enabled,
      .on_txt = "<    On    >", .off_txt = "<    Off    >" },
    { .title = "Equalizer", .kind = RK_SCREEN, .screen = SCREEN_EQ },
};

static const row_desc_t SEC_SCREENS[] = {
    { .title = "BT Screen",    .kind = RK_TOGGLE, .tget = get_bt_show,    .tset = settings_set_bt_show_screen,
      .on_txt = "<    Show    >", .off_txt = "<    Hide    >" },
    { .title = "SD Screen",    .kind = RK_TOGGLE, .tget = get_sd_show,    .tset = settings_set_sd_show_screen,
      .on_txt = "<    Show    >", .off_txt = "<    Hide    >" },
    { .title = "Radio Screen", .kind = RK_TOGGLE, .tget = get_radio_show, .tset = settings_set_radio_show_screen,
      .on_txt = "<    Show    >", .off_txt = "<    Hide    >" },
#if !CONFIG_TOUCH_NONE
    { .title = "Radio Layout", .kind = RK_ACTION, .action = act_radio_layout, .on_txt = "Open >" },
#endif
};

static const row_desc_t SEC_SYSTEM[] = {
#if !CONFIG_TOUCH_NONE
    // Interactive WiFi setup (scan → pick → keyboard) — reachable at runtime so
    // the network can be switched while connected. No-touch panels compile the
    // static AP-info variant of that screen, which would mislead in STA mode,
    // so they don't get the entry (web portal covers them).
    { .title = "WiFi",    .kind = RK_SCREEN, .screen = SCREEN_WIFI },
#endif
    { .title = "Restart", .kind = RK_ACTION, .action = act_restart, .on_txt = "Press >" },
};

typedef struct {
    const char       *title;
    const row_desc_t *rows;
    int               n_rows;
} section_def_t;

static const section_def_t SECTIONS[SECTION_COUNT] = {
    [SECTION_MENU]    = { "Settings", SEC_MENU,    N_ROWS(SEC_MENU)    },
    [SECTION_DISPLAY] = { "Display",  SEC_DISPLAY, N_ROWS(SEC_DISPLAY) },
    [SECTION_AUDIO]   = { "Audio",    SEC_AUDIO,   N_ROWS(SEC_AUDIO)   },
    [SECTION_SCREENS] = { "Screens",  SEC_SCREENS, N_ROWS(SEC_SCREENS) },
    [SECTION_SYSTEM]  = { "System",   SEC_SYSTEM,  N_ROWS(SEC_SYSTEM)  },
};

/* ── forward declarations (mutually-recursive UI helpers) ───────────────── */

static void rebuild_section(void);
static void update_focus_visuals(void);
static void update_hint(void);
static void refresh_row_label(int i);
static void refresh_all_labels(void);
static void row_click_cb(lv_event_t *e);
static void slider_released_cb(lv_event_t *e);

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

/* ── focus / hint / labels ──────────────────────────────────────────────── */

static void update_focus_visuals(void)
{
    const ui_theme_colors_t *th = theme_get();
    for (int i = 0; i < s_count; i++) {
        if (!s_row_obj[i]) continue;
        bool focused = (i == s_focus);
        uint32_t c = focused ? th->accent : th->text_muted;
        int w = (focused && s_editing) ? 4 : 2;
        lv_obj_set_style_border_color(s_row_obj[i], lv_color_hex(c), LV_PART_MAIN);
        lv_obj_set_style_border_width(s_row_obj[i], w, LV_PART_MAIN);
    }
    if (s_focus >= 0 && s_focus < s_count && s_row_obj[s_focus]) {
        // On the first row, snap fully to the top so the scrollable header
        // (title / memory bar) comes back into view; scroll_to_view alone
        // never returns past the focused row.
        if (s_focus == 0) {
            lv_obj_scroll_to_y(s_rows_cont, 0, LV_ANIM_ON);
        } else {
            lv_obj_scroll_to_view(s_row_obj[s_focus], LV_ANIM_ON);
        }
    }
}

static void update_hint(void)
{
    if (!s_hint) return;
    row_kind_t k = (s_focus >= 0 && s_focus < s_count) ? s_rows[s_focus].kind : RK_TOGGLE;
    if (s_editing) {
        lv_label_set_text(s_hint, "turn=change  press=done  swipe>=back");
    } else if (k == RK_SCREEN || k == RK_SECTION) {
        lv_label_set_text(s_hint, "turn=nav  press=open  swipe<>/long=back");
    } else {
        lv_label_set_text(s_hint, "turn=nav  press=edit  swipe<>/long=back");
    }
}

static void refresh_row_label(int i)
{
    if (i < 0 || i >= s_count || !s_row_val[i]) return;
    const row_desc_t *d = &s_rows[i];
    char buf[24];
    switch (d->kind) {
        case RK_TOGGLE:
            lv_label_set_text(s_row_val[i], d->tget() ? d->on_txt : d->off_txt);
            break;
        case RK_SLIDER: {
            int v = d->sget();
            d->sfmt(buf, sizeof(buf), v);
            lv_label_set_text(s_row_val[i], buf);
            if (s_row_slider[i]) lv_slider_set_value(s_row_slider[i], v, LV_ANIM_OFF);
            break;
        }
        case RK_SCREEN:  lv_label_set_text(s_row_val[i], "Open >"); break;
        case RK_SECTION: lv_label_set_text(s_row_val[i], ">");      break;
        case RK_ACTION:  lv_label_set_text(s_row_val[i], d->on_txt ? d->on_txt : ">"); break;
    }
}

static void refresh_all_labels(void)
{
    for (int i = 0; i < s_count; i++) refresh_row_label(i);
}

/* ── actions ────────────────────────────────────────────────────────────── */

static void rebuild_async_cb(void *p)
{
    (void)p;
    rebuild_section();
}

static void enter_section(settings_section_t sec)
{
    s_section = sec;
    s_focus   = 0;
    s_editing = false;
    /* Deferred: entering a section may be triggered from a row's own LVGL
       click callback, and rebuild_section() deletes that very row. Running it
       after the event finishes avoids freeing the object mid-dispatch. */
    lv_async_call(rebuild_async_cb, NULL);
}

static void row_activate(int i)
{
    if (i < 0 || i >= s_count) return;
    const row_desc_t *d = &s_rows[i];
    switch (d->kind) {
        case RK_SECTION:
            enter_section(d->section);
            break;
        case RK_SCREEN:
            ui_navigate(d->screen);
            break;
        case RK_TOGGLE:
            d->tset(!d->tget());
            refresh_row_label(i);
            break;
        case RK_ACTION:
            if (d->action) d->action();
            break;
        case RK_SLIDER:
            s_editing = !s_editing;
            update_focus_visuals();
            update_hint();
            break;
    }
}

/* ── touch: click on a row ──────────────────────────────────────────────── */

static void row_click_cb(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);

    /* tap on a non-focused row → just focus it (and leave edit mode) */
    if (i != s_focus) {
        s_focus = i;
        s_editing = false;
        update_focus_visuals();
        update_hint();
        return;
    }
    /* tap on the already focused row → action */
    row_activate(i);
}

/* ── touch: slider release ──────────────────────────────────────────────── */

static void slider_released_cb(lv_event_t *e)
{
    lv_obj_t *sld = lv_event_get_target(e);
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i < 0 || i >= s_count || s_rows[i].kind != RK_SLIDER) return;

    const row_desc_t *d = &s_rows[i];
    int v = (int)lv_slider_get_value(sld);
    /* quantize to the encoder step so touch and rotation agree */
    v = (v + d->vstep / 2) / d->vstep * d->vstep;
    d->sset(v);
    refresh_row_label(i);
    if (s_focus != i) {
        s_focus = i;
        s_editing = false;
        update_focus_visuals();
        update_hint();
    }
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
    // Card padding from the profile — short panels use a tight value so the
    // label's content box doesn't collapse (cf. settings_row_pad).
    lv_obj_set_style_pad_all(row, p->settings_row_pad, LV_PART_MAIN);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(row);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_font(lbl, p->settings_row_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, lv_color_hex(th->text_secondary), LV_PART_MAIN);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 0, 0);

    return row;
}

/* y of the i-th row, relative to the first row. Reproduces the original spacing:
   row0/1 use the row1→row2 gap, row2 the row1→row3 gap, the rest step uniformly. */
static int row_y_offset(const ui_profile_t *p, int idx)
{
    int step = p->settings_row2_y - p->settings_row1_y;
    if (idx == 0) return 0;
    if (idx == 1) return step;
    return (p->settings_row3_y - p->settings_row1_y) + (idx - 2) * step;
}

static void build_row(int i, const ui_profile_t *p, const ui_theme_colors_t *th, int y)
{
    const row_desc_t *d = &s_rows[i];

    lv_obj_t *row = make_row(s_rows_cont, y, d->title);
    lv_obj_add_event_cb(row, row_click_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    s_row_obj[i] = row;

    if (d->kind == RK_SLIDER && p->settings_show_slider) {
        lv_obj_t *sld = lv_slider_create(row);
        lv_obj_set_size(sld, p->settings_slider_w, p->settings_slider_h);
        lv_obj_align(sld, LV_ALIGN_BOTTOM_LEFT, 0, -2);
        lv_slider_set_range(sld, d->vmin, d->vmax);
        lv_obj_set_style_bg_color(sld, lv_color_hex(th->accent),     LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(sld, lv_color_hex(th->text_muted), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(sld, LV_OPA_COVER, LV_PART_MAIN);
        /* Keep press ownership on the slider even if the finger drifts outside
           its bounds during drag — otherwise LV_EVENT_RELEASED is routed to
           whichever widget happens to be under the finger on release. */
        lv_obj_add_flag(sld, LV_OBJ_FLAG_PRESS_LOCK);
        lv_obj_add_event_cb(sld, slider_released_cb, LV_EVENT_RELEASED, (void *)(intptr_t)i);
        s_row_slider[i] = sld;
    }

    lv_obj_t *val = lv_label_create(row);
    lv_obj_set_style_text_font(val, p->settings_value_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(val, lv_color_hex(th->text_primary), LV_PART_MAIN);
    lv_obj_align(val, (d->kind == RK_SLIDER) ? LV_ALIGN_BOTTOM_RIGHT : LV_ALIGN_BOTTOM_MID, 0, 0);
    s_row_val[i] = val;
}

/* Rebuild the row list for the current section. Clears the scroll container
   (which destroys the in-list title / memory bar) and recreates the header +
   rows from the section's descriptor table. */
static void rebuild_section(void)
{
    if (!s_rows_cont) return;   // screen was destroyed before a deferred rebuild ran
    const ui_theme_colors_t *th = theme_get();
    const ui_profile_t      *p  = ui_profile_get();

    lv_obj_clean(s_rows_cont);
    memset(s_row_obj,    0, sizeof(s_row_obj));
    memset(s_row_val,    0, sizeof(s_row_val));
    memset(s_row_slider, 0, sizeof(s_row_slider));
    s_mem_bar = NULL;

    const section_def_t *def = &SECTIONS[s_section];
    s_rows  = def->rows;
    s_count = def->n_rows;
    if (s_count > MAX_ROWS) s_count = MAX_ROWS;

    bool title_in_list = p->settings_title_in_list;
    int  title_h   = lv_font_get_line_height(p->settings_title_font);
    int  mem_bar_h = lv_font_get_line_height(p->settings_hint_font);
    int  header_y  = 0;

    /* title — when it scrolls with the list it is recreated each rebuild; the
       fixed variant lives in the parent (see settings_create) and only its
       text changes here. */
    if (title_in_list) {
        s_title = lv_label_create(s_rows_cont);
        lv_obj_set_style_text_font(s_title, p->settings_title_font, LV_PART_MAIN);
        lv_obj_set_style_text_color(s_title, lv_color_hex(th->accent), LV_PART_MAIN);
        lv_obj_align(s_title, LV_ALIGN_TOP_MID, 0, header_y);
        header_y += title_h + 1;
    }
    if (s_title) lv_label_set_text(s_title, def->title);

    /* System section carries the read-only memory/CPU/RSSI bar in its header. */
    if (s_section == SECTION_SYSTEM) {
        s_mem_bar = lv_label_create(s_rows_cont);
        lv_obj_set_style_text_font(s_mem_bar, p->settings_hint_font, LV_PART_MAIN);
        lv_obj_set_style_text_color(s_mem_bar, lv_color_hex(th->text_muted), LV_PART_MAIN);
        lv_obj_align(s_mem_bar, LV_ALIGN_TOP_MID, 0, header_y);
        update_mem_bar();
        header_y += mem_bar_h + 2;
    }

    int row_base = header_y;
    for (int i = 0; i < s_count; i++) {
        build_row(i, p, th, row_base + row_y_offset(p, i));
    }

    if (s_focus >= s_count) s_focus = s_count - 1;
    if (s_focus < 0)        s_focus = 0;

    refresh_all_labels();
    update_focus_visuals();
    update_hint();
}

/* ── lifecycle ──────────────────────────────────────────────────────────── */

static void settings_create(lv_obj_t *parent)
{
    s_root = parent;
    const ui_theme_colors_t *th = theme_get();
    const ui_profile_t      *p  = ui_profile_get();

    lv_obj_set_style_bg_color(parent, lv_color_hex(th->bg_primary), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, LV_PART_MAIN);

    int title_h = lv_font_get_line_height(p->settings_title_font);
    int hint_h  = lv_font_get_line_height(p->settings_hint_font);
    bool title_in_list = p->settings_title_in_list;

    /* When the title scrolls with the list (small panels) the container fills
       from the very top; otherwise it starts below the fixed title. */
    int rows_start_y = title_in_list ? 0 : (p->settings_title_y + title_h + 2);
    int rows_cont_h  = DISPLAY_HEIGHT + p->settings_hint_y - hint_h - rows_start_y - 4;

    /* ── scrollable rows container ── */
    s_rows_cont = lv_obj_create(parent);
    lv_obj_set_size(s_rows_cont, p->settings_row_w + 12, rows_cont_h);
    lv_obj_align(s_rows_cont, LV_ALIGN_TOP_MID, 0, rows_start_y);
    lv_obj_set_style_bg_opa(s_rows_cont, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_rows_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_rows_cont, 0, LV_PART_MAIN);
    lv_obj_set_scroll_dir(s_rows_cont, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_rows_cont, LV_SCROLLBAR_MODE_AUTO);

    /* fixed title lives in the parent and survives section rebuilds; the
       in-list title is created per rebuild inside rebuild_section(). */
    if (!title_in_list) {
        s_title = lv_label_create(parent);
        lv_obj_set_style_text_font(s_title, p->settings_title_font, LV_PART_MAIN);
        lv_obj_set_style_text_color(s_title, lv_color_hex(th->accent), LV_PART_MAIN);
        lv_obj_align(s_title, LV_ALIGN_TOP_MID, 0, p->settings_title_y);
    } else {
        s_title = NULL;
    }

    /* hint */
    s_hint = lv_label_create(parent);
    lv_obj_set_style_text_font(s_hint, p->settings_hint_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_hint, lv_color_hex(th->text_muted), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_hint, lv_color_hex(th->bg_primary), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_hint, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(s_hint, 4, LV_PART_MAIN);
    lv_obj_align(s_hint, LV_ALIGN_BOTTOM_MID, 0, p->settings_hint_y);

    s_section = SECTION_MENU;
    s_focus   = 0;
    s_editing = false;
    rebuild_section();

    s_mem_timer = lv_timer_create(mem_timer_cb, 1000, NULL);

    ESP_LOGI(TAG, "Created (theme=%d)", theme_current());
}

static void settings_destroy(void)
{
    if (s_mem_timer) { lv_timer_delete(s_mem_timer); s_mem_timer = NULL; }
    s_root = s_title = s_rows_cont = s_hint = s_mem_bar = NULL;
    s_rows = NULL;
    s_count = 0;
    memset(s_row_obj,    0, sizeof(s_row_obj));
    memset(s_row_val,    0, sizeof(s_row_val));
    memset(s_row_slider, 0, sizeof(s_row_slider));
    ESP_LOGI(TAG, "Destroyed");
}

/* ── events & input ─────────────────────────────────────────────────────── */

static void settings_on_event(const ui_event_t *ev)
{
    if (ev->type == UI_EVT_STATE_CHANGED) {
        refresh_all_labels();
    }
}

static void settings_on_input(ui_input_t input)
{
    switch (input) {

        case UI_INPUT_ENCODER_CW:
        case UI_INPUT_ENCODER_CCW: {
            int delta = (input == UI_INPUT_ENCODER_CW) ? 1 : -1;

            if (s_editing && s_focus < s_count && s_rows[s_focus].kind == RK_SLIDER) {
                const row_desc_t *d = &s_rows[s_focus];
                int v = d->sget() + delta * d->vstep;
                if (v < d->vmin) v = d->vmin;
                if (v > d->vmax) v = d->vmax;
                d->sset(v);
                refresh_row_label(s_focus);
            } else {
                int next = s_focus + delta;
                if (next < 0)        next = 0;
                if (next >= s_count) next = s_count - 1;
                if (next != s_focus) {
                    s_focus = next;
                    update_focus_visuals();
                    update_hint();
                }
            }
            break;
        }

        case UI_INPUT_ENCODER_PRESS:
            row_activate(s_focus);
            break;

        case UI_INPUT_ENCODER_LONG_PRESS:
        case UI_INPUT_SWIPE_RIGHT:
        case UI_INPUT_SWIPE_LEFT:
            /* one level back: section → menu, menu → leave settings */
            if (s_section != SECTION_MENU) {
                enter_section(SECTION_MENU);
            } else {
                ui_navigate(s_return);
            }
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
    if (s_title) lv_obj_set_style_text_color(s_title, lv_color_hex(th->accent), LV_PART_MAIN);

    for (int i = 0; i < s_count; i++) {
        if (s_row_obj[i]) {
            lv_obj_set_style_bg_color(s_row_obj[i], lv_color_hex(th->bg_secondary), LV_PART_MAIN);
            /* child[0] = title label added in make_row */
            lv_obj_t *lbl = lv_obj_get_child(s_row_obj[i], 0);
            if (lbl) lv_obj_set_style_text_color(lbl, lv_color_hex(th->text_secondary), LV_PART_MAIN);
        }
        if (s_row_val[i])
            lv_obj_set_style_text_color(s_row_val[i], lv_color_hex(th->text_primary), LV_PART_MAIN);
        if (s_row_slider[i]) {
            lv_obj_set_style_bg_color(s_row_slider[i], lv_color_hex(th->accent),     LV_PART_INDICATOR);
            lv_obj_set_style_bg_color(s_row_slider[i], lv_color_hex(th->text_muted), LV_PART_MAIN);
        }
    }

    if (s_mem_bar) lv_obj_set_style_text_color(s_mem_bar, lv_color_hex(th->text_muted), LV_PART_MAIN);
    if (s_hint) {
        lv_obj_set_style_text_color(s_hint, lv_color_hex(th->text_muted), LV_PART_MAIN);
        lv_obj_set_style_bg_color(s_hint, lv_color_hex(th->bg_primary), LV_PART_MAIN);
    }

    update_focus_visuals();   /* updates border accent/text_muted */
    refresh_all_labels();     /* refreshes text after theme change */
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
