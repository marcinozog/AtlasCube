#include "screen_wifi_ap.h"
#include "ui_screen.h"
#include "ui_events.h"
#include "ui_manager.h"
#include "theme.h"
#include "ui_profile.h"
#include "fonts/ui_fonts.h"
#include "wifi_manager.h"
#include "settings.h"
#include "defines.h"
#include "lvgl.h"
#include "esp_system.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "SCR_WIFI";

static lv_obj_t *s_root = NULL;

// ─────────────────────────────────────────────────────────────────────────────
#if CONFIG_TOUCH_NONE
// ─────────────────────────────────────────────────────────────────────────────
// No-touch panels (mono OLED / encoder): keep the static info screen — an
// on-screen keyboard has nowhere to be tapped. WiFi is configured over the web
// portal at 192.168.4.1.
// ─────────────────────────────────────────────────────────────────────────────

static void add_row(lv_obj_t *parent,
                    const char *key, const char *value,
                    uint32_t val_color,
                    int y_offset,
                    const ui_theme_colors_t *th,
                    const ui_profile_t *p)
{
    lv_obj_t *k = lv_label_create(parent);
    lv_label_set_text(k, key);
    lv_obj_set_style_text_font(k, p->wifi_key_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(k, lv_color_hex(th->text_muted), LV_PART_MAIN);
    lv_obj_align(k, LV_ALIGN_TOP_LEFT, 0, y_offset);

    lv_obj_t *v = lv_label_create(parent);
    lv_label_set_text(v, value);
    lv_obj_set_style_text_font(v, p->wifi_value_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(v, lv_color_hex(val_color), LV_PART_MAIN);
    lv_obj_align(v, LV_ALIGN_TOP_RIGHT, 0, y_offset);
}

static void wifi_create(lv_obj_t *parent)
{
    s_root = parent;
    const ui_theme_colors_t *th = theme_get();
    const ui_profile_t      *p  = ui_profile_get();

    // Background
    lv_obj_set_style_bg_color(parent, lv_color_hex(th->bg_primary), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, LV_PART_MAIN);

    // ── Title ─────────────────────────────────────────────────────────────────
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title,  "Connect to AtlasCube");
    lv_obj_set_style_text_font(title, p->wifi_title_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_hex(th->accent), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, p->wifi_title_y);

    // ── Card (bg_secondary, rounded) ─────────────────────────────────────────
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, p->wifi_card_w, p->wifi_card_h);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, p->wifi_card_y);
    lv_obj_set_style_bg_color(card, lv_color_hex(th->bg_secondary), LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(card, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(card, p->wifi_card_pad_hor, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(card, p->wifi_card_pad_ver, LV_PART_MAIN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    add_row(card, "SSID",     wifi_get_ap_ssid(), th->text_primary, 0,               th, p);
    add_row(card, "Password", wifi_get_ap_pass(), th->text_primary, p->wifi_row2_y,  th, p);
    add_row(card, "Address",  "192.168.4.1",      th->accent,       p->wifi_row3_y,  th, p);

    // ── Hint at the bottom of the screen ─────────────────────────────────────
    lv_obj_t *hint = lv_label_create(parent);
    lv_label_set_text(hint, "Open browser to configure WiFi:\nhttp://192.168.4.1");
    lv_obj_set_style_text_font(hint, p->wifi_hint_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(hint, lv_color_hex(th->text_muted), LV_PART_MAIN);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, p->wifi_hint_y);

    ESP_LOGI(TAG, "Created (no-touch)");
}

static void wifi_destroy(void) { s_root = NULL; }
static void wifi_on_event(const ui_event_t *ev) { (void)ev; }
static void wifi_on_input(ui_input_t input) { (void)input; }  // no keyboard here

#else  // !CONFIG_TOUCH_NONE
// ─────────────────────────────────────────────────────────────────────────────
// Touch panels: interactive provisioning — scan nearby WiFi, pick one, type the
// password on an on-screen keyboard, save and reboot to connect.
// ─────────────────────────────────────────────────────────────────────────────

static lv_obj_t      *s_list        = NULL;   // scanned-network rows
static lv_obj_t      *s_scan_btn    = NULL;
static lv_obj_t      *s_spinner     = NULL;
static lv_obj_t      *s_status      = NULL;   // muted status/hint line
static lv_obj_t      *s_overlay     = NULL;   // password modal (on top layer)

static wifi_scan_ap_t s_aps[WIFI_SCAN_MAX_AP];
static int            s_ap_count    = 0;
static char           s_sel_ssid[33] = {0};
static bool           s_sel_secure   = false;
static int            s_focus        = -1;   // encoder-highlighted network row

// ── Encoder → LVGL bridge (password overlay only) ────────────────────────────
// The project routes the encoder through its own ui_input bus, not an LVGL
// indev, so the on-screen keyboard can't be driven by the knob on its own.
// While the password overlay is open we spin up a throwaway ENCODER indev whose
// read_cb drains an accumulator that wifi_on_input() fills, and drop the keyboard
// into a group. That makes lv_keyboard's built-in encoder navigation work with
// no change to encoder.c or ui_manager. Both are torn down in close_overlay().
// Safe without locks: wifi_on_input (writer) and enc_read_cb (reader) both run
// on the LVGL task.
static lv_indev_t    *s_enc_indev    = NULL;
static lv_group_t    *s_enc_group    = NULL;
static int            s_enc_diff     = 0;    // rotation steps since last read
static int            s_enc_press    = 0;    // >0 → report PRESSED for N reads

static void enc_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    data->enc_diff = (int16_t)s_enc_diff;
    s_enc_diff = 0;
    if (s_enc_press > 0) { data->state = LV_INDEV_STATE_PRESSED; s_enc_press--; }
    else                   data->state = LV_INDEV_STATE_RELEASED;
}

// ── Simplified keyboard layout ───────────────────────────────────────────────
// Digits stay on a dedicated top row so letters AND numbers are visible at once
// (no mode switching for them). Symbols hide behind one "1#" key; "ABC"/"abc"
// toggle case, and the control keys keep the built-in behaviour (matched by text
// in lv_keyboard_def_event_cb). Fewer keys than the stock map → larger targets.
#define KB_CTRL LV_KEYBOARD_CTRL_BUTTON_FLAGS

static const char * const kb_map_lc[] = {
    "1","2","3","4","5","6","7","8","9","0","\n",
    "q","w","e","r","t","y","u","i","o","p","\n",
    "a","s","d","f","g","h","j","k","l",LV_SYMBOL_BACKSPACE,"\n",
    "ABC","z","x","c","v","b","n","m","1#","\n",
    " ",LV_SYMBOL_OK,""
};
static const char * const kb_map_uc[] = {
    "1","2","3","4","5","6","7","8","9","0","\n",
    "Q","W","E","R","T","Y","U","I","O","P","\n",
    "A","S","D","F","G","H","J","K","L",LV_SYMBOL_BACKSPACE,"\n",
    "abc","Z","X","C","V","B","N","M","1#","\n",
    " ",LV_SYMBOL_OK,""
};
static const char * const kb_map_spec[] = {
    "1","2","3","4","5","6","7","8","9","0","\n",
    "+","-","*","/","=","%","!","?","@","#","\n",
    "(",")","_","&",";",":","'","\"",",",LV_SYMBOL_BACKSPACE,"\n",
    "abc",".","<",">","[","]","{","}","\\","\n",
    " ",LV_SYMBOL_OK,""
};
// Shared control map for the two text layouts (identical geometry).
static const lv_buttonmatrix_ctrl_t kb_ctrl_text[] = {
    1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1, KB_CTRL | 2,
    KB_CTRL | 2, 1,1,1,1,1,1,1, KB_CTRL | 2,
    6, KB_CTRL | 2
};
static const lv_buttonmatrix_ctrl_t kb_ctrl_spec[] = {
    1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1, KB_CTRL | 2,
    KB_CTRL | 2, 1,1,1,1,1,1,1,1,
    6, KB_CTRL | 2
};

// Posted from the WiFi event task → funnel into the UI queue so the results are
// consumed on the LVGL thread (see wifi_on_event).
static void scan_done_thunk(void)
{
    ui_event_t ev = { .type = UI_EVT_WIFI_SCAN_DONE };
    ui_event_send(&ev);
}

static void set_status(const char *txt)
{
    if (s_status) lv_label_set_text(s_status, txt);
}

// ── Password overlay ─────────────────────────────────────────────────────────

static void reboot_timer_cb(lv_timer_t *t)
{
    (void)t;
    esp_restart();
}

static void close_overlay(void)
{
    if (s_enc_indev) { lv_indev_delete(s_enc_indev); s_enc_indev = NULL; }
    if (s_enc_group) { lv_group_delete(s_enc_group); s_enc_group = NULL; }
    s_enc_diff = 0;
    s_enc_press = 0;
    if (s_overlay) { lv_obj_del(s_overlay); s_overlay = NULL; }
}

static void kb_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t       *kb   = lv_event_get_target(e);
    lv_obj_t       *ta   = lv_keyboard_get_textarea(kb);

    if (code == LV_EVENT_CANCEL) {
        close_overlay();
        return;
    }
    if (code != LV_EVENT_READY) return;

    const char *pass = ta ? lv_textarea_get_text(ta) : "";
    if (s_sel_secure && (!pass || pass[0] == '\0')) {
        set_status("Password required");     // keep overlay open
        return;
    }

    ESP_LOGI(TAG, "Saving WiFi \"%s\" → reboot", s_sel_ssid);
    settings_set_wifi(s_sel_ssid, pass);

    // Replace the overlay content with a reboot notice, then restart shortly so
    // the label has a chance to paint.
    lv_obj_clean(s_overlay);
    const ui_theme_colors_t *th = theme_get();
    const ui_profile_t      *p  = ui_profile_get();
    lv_obj_t *msg = lv_label_create(s_overlay);
    lv_label_set_text(msg, "Saved - restarting...");
    lv_obj_set_style_text_font(msg, p->wifi_title_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(msg, lv_color_hex(th->accent), LV_PART_MAIN);
    lv_obj_center(msg);

    lv_timer_t *t = lv_timer_create(reboot_timer_cb, 900, NULL);
    lv_timer_set_repeat_count(t, 1);
}

static void eye_btn_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    lv_obj_t *ta  = lv_event_get_user_data(e);
    if (!ta) return;
    bool hidden = !lv_textarea_get_password_mode(ta);   // new state after toggle
    lv_textarea_set_password_mode(ta, hidden);
    lv_obj_t *lbl = lv_obj_get_child(btn, 0);
    if (lbl) lv_label_set_text(lbl, hidden ? "Show" : "Hide");
}

static void open_password_overlay(const char *ssid, bool secure)
{
    close_overlay();
    strlcpy(s_sel_ssid, ssid, sizeof(s_sel_ssid));
    s_sel_secure = secure;

    const ui_theme_colors_t *th = theme_get();
    const ui_profile_t      *p  = ui_profile_get();

    // Modal on the top layer so it covers the list and keyboard fully.
    s_overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_overlay, lv_color_hex(th->bg_primary), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_overlay, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_overlay, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_overlay, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_overlay, 6, LV_PART_MAIN);
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(s_overlay);
    lv_label_set_text_fmt(lbl, "Password for %s", ssid);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(lbl, LV_PCT(100));
    lv_obj_set_style_text_font(lbl, p->wifi_value_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, lv_color_hex(th->text_primary), LV_PART_MAIN);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 0, 0);

    int ta_y = lv_font_get_line_height(p->wifi_value_font) + 6;

    lv_obj_t *ta = lv_textarea_create(s_overlay);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_password_mode(ta, true);
    lv_textarea_set_placeholder_text(ta, "Password");
    lv_obj_set_width(ta, LV_PCT(72));
    lv_obj_align(ta, LV_ALIGN_TOP_LEFT, 0, ta_y);
    lv_obj_update_layout(ta);   // resolve field height before aligning the toggle

    // Show/hide-password toggle to the right of the field. Text label (not an
    // LV_SYMBOL) because the _pl fonts carry no FontAwesome glyph range.
    lv_obj_t *eye = lv_button_create(s_overlay);
    lv_obj_set_style_bg_color(eye, lv_color_hex(th->bg_secondary), LV_PART_MAIN);
    lv_obj_set_style_radius(eye, 6, LV_PART_MAIN);
    lv_obj_set_height(eye, lv_obj_get_height(ta));   // match the field's height
    lv_obj_add_event_cb(eye, eye_btn_cb, LV_EVENT_CLICKED, ta);
    lv_obj_t *eye_lbl = lv_label_create(eye);
    lv_label_set_text(eye_lbl, "Show");   // field starts hidden → tap to reveal
    lv_obj_set_style_text_font(eye_lbl, p->wifi_key_font, LV_PART_MAIN);
    lv_obj_center(eye_lbl);
    lv_obj_align_to(eye, ta, LV_ALIGN_OUT_RIGHT_MID, 6, 0);

    lv_obj_t *kb = lv_keyboard_create(s_overlay);
    lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_TEXT_LOWER, kb_map_lc,   kb_ctrl_text);
    lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_TEXT_UPPER, kb_map_uc,   kb_ctrl_text);
    lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_SPECIAL,    kb_map_spec, kb_ctrl_spec);
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_keyboard_set_popovers(kb, true);   // magnified preview of the pressed key
    lv_keyboard_set_textarea(kb, ta);

    // Bigger keyboard + gaps + font → larger, easier-to-hit targets.
    lv_obj_set_size(kb, LV_PCT(100), LV_PCT(64));
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    // Builtin montserrat (not a _pl font) so the backspace/OK LV_SYMBOL glyphs
    // actually render — the _pl fonts carry no FontAwesome range. The keyboard
    // only shows ASCII, so losing the Polish glyphs here is irrelevant.
    lv_obj_set_style_text_font(kb, &lv_font_montserrat_14, LV_PART_ITEMS);
    lv_obj_set_style_pad_all(kb, 3, LV_PART_MAIN);
    lv_obj_set_style_pad_row(kb, 5, LV_PART_MAIN);
    lv_obj_set_style_pad_column(kb, 5, LV_PART_MAIN);

    lv_obj_add_event_cb(kb, kb_event_cb, LV_EVENT_READY,  NULL);
    lv_obj_add_event_cb(kb, kb_event_cb, LV_EVENT_CANCEL, NULL);

    // Encoder navigation for the keyboard (harmless when no encoder is wired —
    // wifi_on_input simply never feeds the accumulator). Edit mode is forced so a
    // knob turn moves between keys immediately instead of leaving the widget.
    s_enc_diff  = 0;
    s_enc_press = 0;
    s_enc_group = lv_group_create();
    lv_group_add_obj(s_enc_group, kb);
    lv_group_set_editing(s_enc_group, true);
    s_enc_indev = lv_indev_create();
    lv_indev_set_type(s_enc_indev, LV_INDEV_TYPE_ENCODER);
    lv_indev_set_read_cb(s_enc_indev, enc_read_cb);
    lv_indev_set_group(s_enc_indev, s_enc_group);
}

// ── Network list ─────────────────────────────────────────────────────────────

static void net_row_cb(lv_event_t *e)
{
    lv_obj_t *row = lv_event_get_target(e);
    if (!row) return;
    int idx = lv_obj_get_index(row);
    if (idx < 0 || idx >= s_ap_count) return;
    open_password_overlay(s_aps[idx].ssid, s_aps[idx].secure);
}

static void rebuild_list(void)
{
    if (!s_list) return;
    lv_obj_clean(s_list);

    const ui_theme_colors_t *th = theme_get();
    const ui_profile_t      *p  = ui_profile_get();

    for (int i = 0; i < s_ap_count; i++) {
        lv_obj_t *row = lv_obj_create(s_list);
        lv_obj_set_width(row, LV_PCT(100));
        lv_obj_set_height(row, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(row, lv_color_hex(th->bg_secondary), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(row, 4, LV_PART_MAIN);
        lv_obj_set_style_pad_ver(row, 6, LV_PART_MAIN);
        lv_obj_set_style_pad_hor(row, 8, LV_PART_MAIN);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(row, net_row_cb, LV_EVENT_CLICKED, NULL);

        // Mark open networks explicitly; the rest are assumed secured (the common
        // case) and get a password prompt. No LV_SYMBOL — _pl fonts lack glyphs.
        lv_obj_t *name = lv_label_create(row);
        lv_label_set_text_fmt(name, "%s%s",
                              s_aps[i].ssid, s_aps[i].secure ? "" : "  (open)");
        lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
        lv_obj_set_width(name, LV_PCT(100));
        lv_obj_set_style_text_font(name, p->wifi_value_font, LV_PART_MAIN);
        lv_obj_set_style_text_color(name, lv_color_hex(th->text_primary), LV_PART_MAIN);
        lv_obj_align(name, LV_ALIGN_LEFT_MID, 0, 0);
    }
}

// Outline the encoder-focused row so knob users can see the selection. No-op on
// pure-touch use (s_focus stays -1 → every row drawn plain).
static void highlight_focus(void)
{
    if (!s_list) return;
    const ui_theme_colors_t *th = theme_get();
    uint32_t n = lv_obj_get_child_count(s_list);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t *row = lv_obj_get_child(s_list, i);
        bool sel = ((int)i == s_focus);
        lv_obj_set_style_border_width(row, sel ? 2 : 0, LV_PART_MAIN);
        lv_obj_set_style_border_color(row, lv_color_hex(th->accent), LV_PART_MAIN);
    }
    if (s_focus >= 0 && s_focus < (int)n)
        lv_obj_scroll_to_view(lv_obj_get_child(s_list, s_focus), LV_ANIM_ON);
}

static void scan_btn_cb(lv_event_t *e)
{
    (void)e;
    if (wifi_manager_scan_busy()) return;
    s_focus = -1;
    if (s_list)    lv_obj_clean(s_list);
    if (s_spinner) lv_obj_clear_flag(s_spinner, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_state(s_scan_btn, LV_STATE_DISABLED);
    set_status("Scanning...");
    wifi_manager_scan_start();
}

static void wifi_create(lv_obj_t *parent)
{
    s_root = parent;
    s_overlay = NULL;
    const ui_theme_colors_t *th = theme_get();
    const ui_profile_t      *p  = ui_profile_get();

    lv_obj_set_style_bg_color(parent, lv_color_hex(th->bg_primary), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    // ── Title ────────────────────────────────────────────────────────────────
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "WiFi setup");
    lv_obj_set_style_text_font(title, p->wifi_title_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_hex(th->accent), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, p->wifi_title_y);

    // ── Scan button ──────────────────────────────────────────────────────────
    s_scan_btn = lv_button_create(parent);
    lv_obj_set_style_bg_color(s_scan_btn, lv_color_hex(th->accent), LV_PART_MAIN);
    lv_obj_set_style_radius(s_scan_btn, 6, LV_PART_MAIN);
    lv_obj_add_event_cb(s_scan_btn, scan_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *blbl = lv_label_create(s_scan_btn);
    lv_label_set_text(blbl, "Scan networks");
    lv_obj_set_style_text_font(blbl, p->wifi_key_font, LV_PART_MAIN);
    lv_obj_center(blbl);
    lv_obj_align(s_scan_btn, LV_ALIGN_TOP_MID, 0,
                 p->wifi_title_y + lv_font_get_line_height(p->wifi_title_font) + 6);

    // ── Status / hint line ───────────────────────────────────────────────────
    s_status = lv_label_create(parent);
    // Startup hint, overwritten with progress once scanning. In STA mode
    // (entered from Settings → System → WiFi) show the live connection instead
    // of the AP provisioning data, which would be misleading here.
    if (wifi_get_run_mode() == WIFI_RUN_MODE_STA) {
        char ip[16];
        lv_label_set_text_fmt(s_status,
                              "Connected to \"%s\"  IP: %s\n"
                              "Scan to switch network, swipe to go back",
                              settings_get()->wifi.ssid,
                              wifi_get_ip(ip, sizeof(ip)));
    } else {
        lv_label_set_text_fmt(s_status,
                              "AP mode - browser setup: http://192.168.4.1\n"
                              "WiFi \"%s\"  pass: %s\n"
                              "or press the knob to scan networks",
                              wifi_get_ap_ssid(), wifi_get_ap_pass());
    }
    lv_label_set_long_mode(s_status, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_status, LV_PCT(96));
    lv_obj_set_style_text_font(s_status, p->wifi_hint_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_status, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_align(s_status, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align_to(s_status, s_scan_btn, LV_ALIGN_OUT_BOTTOM_MID, 0, 4);

    // ── Network list (fills the remaining area) ──────────────────────────────
    // Measure header height, then give the list the rest of the screen.
    lv_obj_update_layout(parent);
    int top = lv_obj_get_y(s_status) + lv_obj_get_height(s_status) + 4;

    s_list = lv_obj_create(parent);
    lv_obj_set_size(s_list, LV_PCT(100), DISPLAY_HEIGHT - top - 4);
    lv_obj_align(s_list, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_bg_opa(s_list, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_list, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_row(s_list, 4, LV_PART_MAIN);
    lv_obj_set_scroll_dir(s_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_list, LV_SCROLLBAR_MODE_ACTIVE);

    // Spinner shown on top of the list area while scanning.
    s_spinner = lv_spinner_create(parent);
    lv_spinner_set_anim_params(s_spinner, 1000, 60);
    lv_obj_set_size(s_spinner, 40, 40);
    lv_obj_align_to(s_spinner, s_list, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(s_spinner, LV_OBJ_FLAG_HIDDEN);

    wifi_manager_set_scan_done_cb(scan_done_thunk);

    ESP_LOGI(TAG, "Created (touch)");
}

static void wifi_destroy(void)
{
    wifi_manager_set_scan_done_cb(NULL);
    close_overlay();
    s_root = s_list = s_scan_btn = s_spinner = s_status = NULL;
    s_ap_count = 0;
    s_focus    = -1;
}

static void wifi_on_event(const ui_event_t *ev)
{
    if (!ev || ev->type != UI_EVT_WIFI_SCAN_DONE) return;
    if (!s_root) return;   // screen already gone

    s_ap_count = wifi_manager_scan_get(s_aps, WIFI_SCAN_MAX_AP);
    if (s_spinner)  lv_obj_add_flag(s_spinner, LV_OBJ_FLAG_HIDDEN);
    if (s_scan_btn) lv_obj_clear_state(s_scan_btn, LV_STATE_DISABLED);
    if (s_ap_count > 0) set_status("Pick a network - tap or turn the knob");
    else                set_status("No networks found - press knob to rescan");
    rebuild_list();
    s_focus = (s_ap_count > 0) ? 0 : -1;
    highlight_focus();
}

static void wifi_on_input(ui_input_t input)
{
    // Password overlay open → funnel the knob into the LVGL keyboard through the
    // throwaway encoder indev; long-press backs out of the overlay.
    if (s_overlay) {
        switch (input) {
            case UI_INPUT_ENCODER_CW:         s_enc_diff++;      break;
            case UI_INPUT_ENCODER_CCW:        s_enc_diff--;      break;
            case UI_INPUT_ENCODER_PRESS:      s_enc_press = 2;   break; // press+release
            case UI_INPUT_ENCODER_LONG_PRESS: close_overlay();   break;
            default: break;
        }
        return;
    }

    // Network list: knob moves the highlight, press opens the selected AP,
    // long-press (re)scans. Press on an empty list scans too.
    switch (input) {
        case UI_INPUT_ENCODER_CW:
        case UI_INPUT_ENCODER_CCW: {
            if (s_ap_count <= 0) break;
            int next = s_focus + ((input == UI_INPUT_ENCODER_CW) ? 1 : -1);
            if (next < 0)            next = 0;
            if (next >= s_ap_count)  next = s_ap_count - 1;
            if (next != s_focus) { s_focus = next; highlight_focus(); }
            break;
        }
        case UI_INPUT_ENCODER_PRESS:
            if (s_ap_count <= 0) { scan_btn_cb(NULL); break; }
            if (s_focus < 0 || s_focus >= s_ap_count) s_focus = 0;
            open_password_overlay(s_aps[s_focus].ssid, s_aps[s_focus].secure);
            break;
        case UI_INPUT_ENCODER_LONG_PRESS:
            scan_btn_cb(NULL);
            break;
        case UI_INPUT_SWIPE_LEFT:
        case UI_INPUT_SWIPE_RIGHT:
            // Entered from the Settings menu → swipe backs out. During boot
            // provisioning (AP mode) this screen is the only destination, so
            // the gesture is ignored there.
            if (wifi_get_run_mode() == WIFI_RUN_MODE_STA)
                ui_navigate(SCREEN_SETTINGS);
            break;
        default:
            break;
    }
}

#endif  // CONFIG_TOUCH_NONE

// ─────────────────────────────────────────────────────────────────────────────

static void wifi_apply_theme(void)              { /* static screen, theme fixed in AP */ }

const ui_screen_t screen_wifi = {
    .create      = wifi_create,
    .destroy     = wifi_destroy,
    .apply_theme = wifi_apply_theme,
    .on_event    = wifi_on_event,
    .on_input    = wifi_on_input,
    .name        = "wifi",
};
