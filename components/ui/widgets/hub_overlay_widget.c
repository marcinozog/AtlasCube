#include "hub_overlay_widget.h"
#include "defines.h"
#include "ui_profile.h"          // DISPLAY_WIDTH / DISPLAY_HEIGHT
#include "ui_manager.h"          // ui_navigate
#include "ui_events.h"           // SCREEN_*
#include "screen_playlist.h"     // screen_playlist_set_return
#include "screen_sd_browser.h"   // screen_sd_browser_set_return
#include "screen_settings.h"     // screen_settings_set_return
#include "theme.h"
#include "settings.h"
#include "app_state.h"
#include "radio_service.h"       // radio_play_index / radio_stop
#include "sd_player.h"
#include "bt.h"                  // bt_play / bt_pause (AVRCP transport)
#include "fonts/ui_fonts.h"
#include "lvgl.h"
#include "esp_log.h"

static const char *TAG = "HUB_OVL";

#define AUTOHIDE_MS 2500          // a touch longer than controls_overlay: more buttons to read

// Buttons in creation = encoder-navigation order:
//   vol-  prev  play  next  vol+   source  playlist  sd  settings
#define HUB_BTN_COUNT 9
#define HUB_FOCUS_DEFAULT 2          // play (center) — first focus when shown

static lv_obj_t   *s_parent   = NULL;
static lv_obj_t   *s_overlay  = NULL;   // dim layer + buttons
static lv_timer_t *s_timer    = NULL;
static lv_obj_t   *s_play_lbl   = NULL;
static lv_obj_t   *s_source_lbl = NULL;   // glyph mirrors the active source (mode_indicator)
static lv_obj_t   *s_btns[HUB_BTN_COUNT] = {0};  // focusable buttons, in nav order
static int         s_focus    = -1;       // index into s_btns, -1 = none
static controls_overlay_mode_t s_mode = CTRL_OVL_MODE_RADIO;

// Button identifiers — transport (cross-equivalent) + action row.
typedef enum {
    BTN_VOL_UP, BTN_VOL_DN, BTN_PREV, BTN_NEXT, BTN_PLAY,
    BTN_SOURCE, BTN_PLAYLIST, BTN_SD, BTN_SETTINGS,
} btn_id_t;

#define BTN_EVT_SHORT   (1u << 0)   // LV_EVENT_SHORT_CLICKED
#define BTN_EVT_REPEAT  (1u << 1)   // LV_EVENT_LONG_PRESSED_REPEAT (vol hold)

static void overlay_hide(void);

// Center button glyph for the current mode/state: SD & radio show STOP while
// playing and PLAY when stopped; BT's play button is a no-op so it stays PLAY.
static const char *play_symbol_for_mode(controls_overlay_mode_t mode)
{
    if (mode == CTRL_OVL_MODE_SD) {
        return sd_player_is_active() ? LV_SYMBOL_STOP : LV_SYMBOL_PLAY;
    }
    if (mode == CTRL_OVL_MODE_BT) return app_state_get()->bt_playing ? LV_SYMBOL_STOP : LV_SYMBOL_PLAY;
    bool playing = (app_state_get()->radio_state == RADIO_STATE_PLAYING);
    return playing ? LV_SYMBOL_STOP : LV_SYMBOL_PLAY;
}

// The source button toggles BT, so its glyph shows the *target* it switches to
// (the opposite of the current source), not the current one: BT when BT is off
// (tap → engage BT), audio when BT is on (tap → back to radio/SD).
static void source_update(void)
{
    if (!s_source_lbl) return;
    const ui_theme_colors_t *th = theme_get();
    if (!app_state_get()->bt_enable) {
        lv_label_set_text(s_source_lbl, LV_SYMBOL_BLUETOOTH);
        lv_obj_set_style_text_color(s_source_lbl, lv_color_hex(th->bt_brand), LV_PART_MAIN);
    } else {
        lv_label_set_text(s_source_lbl, LV_SYMBOL_AUDIO);
        lv_obj_set_style_text_color(s_source_lbl, lv_color_hex(th->accent), LV_PART_MAIN);
    }
}

static void autohide_cb(lv_timer_t *t) { (void)t; overlay_hide(); }

// White ring around the encoder-focused button (the buttons carry no border of
// their own, so this is the only border style in play).
static void focus_highlight(lv_obj_t *btn, bool on)
{
    if (!btn) return;
    lv_obj_set_style_border_color(btn, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_border_opa(btn, on ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, on ? 3 : 0, LV_PART_MAIN);
}

// Move focus to button `idx` (wraps), repaint the ring and reset the auto-hide.
static void focus_set(int idx)
{
    if (idx < 0)               idx = HUB_BTN_COUNT - 1;
    if (idx >= HUB_BTN_COUNT)  idx = 0;
    if (s_focus >= 0) focus_highlight(s_btns[s_focus], false);
    s_focus = idx;
    focus_highlight(s_btns[s_focus], true);
    if (s_timer) lv_timer_reset(s_timer);
}

static void overlay_show(void)
{
    if (!s_overlay) return;
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_overlay);
    if (s_timer) {
        lv_timer_reset(s_timer);
    } else {
        s_timer = lv_timer_create(autohide_cb, AUTOHIDE_MS, NULL);
        lv_timer_set_repeat_count(s_timer, 1);
    }
    focus_set(HUB_FOCUS_DEFAULT);   // start on the center play button
}

static void overlay_hide(void)
{
    if (s_overlay) lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
    if (s_timer) { lv_timer_del(s_timer); s_timer = NULL; }
}

static void parent_clicked_cb(lv_event_t *e)
{
    (void)e;
    if (!s_overlay) return;
    if (lv_obj_has_flag(s_overlay, LV_OBJ_FLAG_HIDDEN)) overlay_show();
}

// Volume +/- for the active source (BT has its own volume).
static void adjust_volume(int delta)
{
    app_state_t *s = app_state_get();
    if (s_mode == CTRL_OVL_MODE_BT) {
        int vol = s->bt_volume + delta;
        if (vol < 0) vol = 0; else if (vol > 100) vol = 100;
        settings_set_bt_volume(vol);
    } else {
        int vol = s->volume + delta;
        if (vol < 0) vol = 0; else if (vol > 100) vol = 100;
        settings_set_volume(vol);
    }
}

static void btn_clicked_cb(lv_event_t *e)
{
    btn_id_t btn = (btn_id_t)(intptr_t)lv_event_get_user_data(e);
    app_state_t *s = app_state_get();

    switch (btn) {
        case BTN_VOL_UP: adjust_volume(+2); break;
        case BTN_VOL_DN: adjust_volume(-2); break;

        case BTN_PREV:
        case BTN_NEXT:
            if (s_mode == CTRL_OVL_MODE_RADIO) {
                radio_play_index(s->curr_index + (btn == BTN_NEXT ? 1 : -1));
            } else if (s_mode == CTRL_OVL_MODE_SD) {
                (btn == BTN_NEXT) ? sd_player_next() : sd_player_prev();
            }
            // BT: no transport from the device side.
            break;

        case BTN_PLAY:
            if (s_mode == CTRL_OVL_MODE_RADIO) {
                if (s->radio_state == RADIO_STATE_STOPPED) {
                    radio_play_index(s->curr_index);
                    lv_label_set_text(s_play_lbl, LV_SYMBOL_STOP);
                } else if (s->radio_state == RADIO_STATE_PLAYING) {
                    radio_stop();
                    lv_label_set_text(s_play_lbl, LV_SYMBOL_PLAY);
                }
            } else if (s_mode == CTRL_OVL_MODE_SD) {
                // Stop/Play toggle: stop keeps the queue so play replays the track.
                if (sd_player_is_active()) {
                    sd_player_stop_keep();
                    lv_label_set_text(s_play_lbl, LV_SYMBOL_PLAY);
                } else {
                    sd_player_resume_current();
                    lv_label_set_text(s_play_lbl, LV_SYMBOL_STOP);
                }
            } else if (s_mode == CTRL_OVL_MODE_BT) {
                // Tell the phone to resume/pause over AVRCP. The glyph follows the
                // real state via app_state->bt_playing; set it optimistically here.
                bool want_play = !app_state_get()->bt_playing;
                want_play ? bt_play() : bt_pause();
                lv_label_set_text(s_play_lbl, want_play ? LV_SYMBOL_STOP : LV_SYMBOL_PLAY);
            }
            break;

        // Source button → switch the audio source to/from BT (like screen_bt's
        // long-press) instead of navigating. Mode + glyph refresh via the screen's
        // STATE_CHANGED handler; vol then drives BT through adjust_volume().
        case BTN_SOURCE: {
            if (app_state_get()->bt_enable) {
                settings_set_bt_enable(false);
            } else {
                settings_set_bt_enable(true);
                bt_play();   // bt_playing updates via the module's play event
            }
            break;
        }

        // Action row → jump to the source lists / settings. Navigation tears down
        // this screen (and the overlay) so no timer reset is needed afterwards.
        case BTN_PLAYLIST: screen_playlist_set_return(SCREEN_HOME);
                           ui_navigate(SCREEN_PLAYLIST);   return;
        case BTN_SD:       screen_sd_browser_set_return(SCREEN_HOME);
                           ui_navigate(SCREEN_SD_BROWSER); return;
        case BTN_SETTINGS: screen_settings_set_return(SCREEN_HOME);
                           ui_navigate(SCREEN_SETTINGS);   return;
    }

    // keep overlay open and reset the auto-hide timer
    if (s_timer) lv_timer_reset(s_timer);
}

static void btn_long_repeat_cb(lv_event_t *e)
{
    btn_id_t btn = (btn_id_t)(intptr_t)lv_event_get_user_data(e);
    if (btn == BTN_VOL_UP) adjust_volume(+2);
    else if (btn == BTN_VOL_DN) adjust_volume(-2);
    if (s_timer) lv_timer_reset(s_timer);
}

// One round button with a (scaled) glyph centered inside. `dx,dy` are offsets
// from the screen center; `gscale` shrinks the 48px glyph to fit small buttons.
static lv_obj_t *make_btn(lv_obj_t *parent, const char *symbol, intptr_t id,
                          int dx, int dy, int size, lv_color_t bg, int gscale,
                          uint32_t evt_mask)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, size, size);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, bg, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_90, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, lv_color_white(), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btn, LV_OPA_50, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
    lv_obj_align(btn, LV_ALIGN_CENTER, dx, dy);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, symbol);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, lv_color_white(), LV_PART_MAIN);
    lv_obj_center(lbl);
    if (gscale != 256) {
        lv_obj_set_style_transform_pivot_x(lbl, lv_pct(50), LV_PART_MAIN);
        lv_obj_set_style_transform_pivot_y(lbl, lv_pct(50), LV_PART_MAIN);
        lv_obj_set_style_transform_scale(lbl, gscale, LV_PART_MAIN);
    }

    if (evt_mask & BTN_EVT_SHORT)
        lv_obj_add_event_cb(btn, btn_clicked_cb, LV_EVENT_SHORT_CLICKED, (void *)id);
    if (evt_mask & BTN_EVT_REPEAT)
        lv_obj_add_event_cb(btn, btn_long_repeat_cb, LV_EVENT_LONG_PRESSED_REPEAT, (void *)id);
    return btn;
}

void hub_overlay_create(lv_obj_t *parent, controls_overlay_mode_t mode)
{
    if (!parent || s_overlay) return;
    s_parent = parent;
    s_mode   = mode;
    const ui_theme_colors_t *th = theme_get();

    // Make the screen background tap-aware so taps in empty areas show the overlay.
    lv_obj_add_flag(parent, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(parent, parent_clicked_cb, LV_EVENT_CLICKED, NULL);

    // Full-screen dim layer — tapping the dim area (outside buttons) hides it.
    s_overlay = lv_obj_create(parent);
    lv_obj_remove_style_all(s_overlay);
    lv_obj_set_size(s_overlay, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_set_pos(s_overlay, 0, 0);
    lv_obj_set_style_bg_color(s_overlay, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_overlay, LV_OPA_60, LV_PART_MAIN);
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_CLICKABLE);   // absorb taps on the dim area
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);

    // Geometry: size both rows to the wider (5-button) transport row so the whole
    // grid stays inside the shorter screen side.
    int side = (DISPLAY_WIDTH < DISPLAY_HEIGHT) ? DISPLAY_WIDTH : DISPLAY_HEIGHT;
    int sz   = side / 5;
    if (sz < 40) sz = 40;
    if (sz > 72) sz = 72;
    int step  = sz + sz / 6;                 // center-to-center within a row
    int row_y = sz / 2 + 6;                  // half-step above/below center
    int gscale = (int)((sz * 0.55f / 48.0f) * 256.0f);   // shrink 48px glyph to ~0.55*sz
    if (gscale > 256) gscale = 256;

    lv_color_t btn_bg    = lv_color_hex(th->bg_secondary);
    lv_color_t play_bg   = lv_color_hex(th->accent);
    lv_color_t act_bg    = lv_color_hex(th->bg_secondary);

    // Row 1 — transport: vol-  prev  play  next  vol+
    // Collect every button into s_btns in this exact order so encoder focus
    // walks left-to-right across row 1 then row 2 (see HUB_BTN_COUNT).
    s_btns[0] = make_btn(s_overlay, LV_SYMBOL_MINUS, BTN_VOL_DN, -2 * step, -row_y, sz, btn_bg, gscale,
        BTN_EVT_SHORT | BTN_EVT_REPEAT);
    s_btns[1] = make_btn(s_overlay, LV_SYMBOL_PREV,  BTN_PREV,   -1 * step, -row_y, sz, btn_bg, gscale,
        BTN_EVT_SHORT);
    lv_obj_t *play_btn = make_btn(s_overlay, play_symbol_for_mode(s_mode), BTN_PLAY,
        0, -row_y, sz, play_bg, gscale, BTN_EVT_SHORT);
    s_btns[2]  = play_btn;
    s_play_lbl = lv_obj_get_child(play_btn, 0);
    s_btns[3] = make_btn(s_overlay, LV_SYMBOL_NEXT,  BTN_NEXT,    1 * step, -row_y, sz, btn_bg, gscale,
        BTN_EVT_SHORT);
    s_btns[4] = make_btn(s_overlay, LV_SYMBOL_PLUS,  BTN_VOL_UP,  2 * step, -row_y, sz, btn_bg, gscale,
        BTN_EVT_SHORT | BTN_EVT_REPEAT);

    // Row 2 — actions: source  playlist  sd  settings (4 buttons, centered).
    // The source button's glyph tracks the active source (see source_update).
    lv_obj_t *src_btn = make_btn(s_overlay, LV_SYMBOL_AUDIO, BTN_SOURCE, -3 * step / 2, row_y,
        sz, act_bg, gscale, BTN_EVT_SHORT);
    s_btns[5]    = src_btn;
    s_source_lbl = lv_obj_get_child(src_btn, 0);
    source_update();
    s_btns[6] = make_btn(s_overlay, LV_SYMBOL_LIST,      BTN_PLAYLIST, -1 * step / 2, row_y, sz, act_bg, gscale,
        BTN_EVT_SHORT);
    s_btns[7] = make_btn(s_overlay, LV_SYMBOL_SD_CARD,   BTN_SD,        1 * step / 2, row_y, sz, act_bg, gscale,
        BTN_EVT_SHORT);
    s_btns[8] = make_btn(s_overlay, LV_SYMBOL_SETTINGS,  BTN_SETTINGS,  3 * step / 2, row_y, sz, act_bg, gscale,
        BTN_EVT_SHORT);

    ESP_LOGI(TAG, "Created (btn=%dpx, step=%dpx)", sz, step);
}

void hub_overlay_show(void) { overlay_show(); }

bool hub_overlay_is_visible(void)
{
    return s_overlay && !lv_obj_has_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
}

void hub_overlay_focus_next(void) { if (hub_overlay_is_visible()) focus_set(s_focus + 1); }
void hub_overlay_focus_prev(void) { if (hub_overlay_is_visible()) focus_set(s_focus - 1); }

void hub_overlay_activate(void)
{
    if (!hub_overlay_is_visible() || s_focus < 0) return;
    // Same path as a touch tap — the button's own SHORT_CLICKED handler runs.
    lv_obj_send_event(s_btns[s_focus], LV_EVENT_SHORT_CLICKED, NULL);
}

void hub_overlay_set_mode(controls_overlay_mode_t mode)
{
    s_mode = mode;
    hub_overlay_refresh();
}

void hub_overlay_refresh(void)
{
    if (s_play_lbl) lv_label_set_text(s_play_lbl, play_symbol_for_mode(s_mode));
    source_update();
}

void hub_overlay_destroy(void)
{
    if (s_timer) { lv_timer_del(s_timer); s_timer = NULL; }
    // Parent is owned by the screen; LVGL frees the children. Drop our refs only.
    s_play_lbl   = NULL;
    s_source_lbl = NULL;
    s_overlay    = NULL;
    s_parent     = NULL;
    for (int i = 0; i < HUB_BTN_COUNT; i++) s_btns[i] = NULL;
    s_focus      = -1;
}
