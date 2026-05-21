#include "controls_overlay_widget.h"
#include "ui_profile.h"
#include "ui_manager.h"
#include "theme.h"
#include "settings.h"
#include "esp_log.h"

static const char *TAG = "CTRL_OVL";

#define AUTOHIDE_MS 1500

static lv_obj_t  *s_parent  = NULL;
static lv_obj_t  *s_overlay = NULL;   // dim layer + buttons
static lv_timer_t *s_timer  = NULL;
static lv_obj_t *s_play_lbl = NULL;

static void overlay_hide(void);

typedef enum { CTRL_VOL_UP, CTRL_VOL_DN, CTRL_PREV, CTRL_NEXT, CTRL_PLAY } ctrl_id_t;

// Which input events a button reacts to. Combine with bitwise OR.
#define CTRL_EVT_SHORT   (1u << 0)   // LV_EVENT_SHORT_CLICKED       → btn_clicked_cb
#define CTRL_EVT_REPEAT  (1u << 1)   // LV_EVENT_LONG_PRESSED_REPEAT → btn_long_repeat_cb

static void autohide_cb(lv_timer_t *t)
{
    (void)t;
    overlay_hide();
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
}

static void overlay_hide(void)
{
    if (s_overlay) lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
    if (s_timer) { lv_timer_del(s_timer); s_timer = NULL; }
}

// LVGL touch callbacks fire on the UI thread but bypass ui_manager's event
// queue, so ui_manager has no way to know the user interacted. We post a
// neutral UI_EVT_INPUT (UI_INPUT_NONE) just so the UI_EVT_INPUT handler in
// ui_manager_run() resets s_last_input_us — otherwise the screensaver fires
// while the user is actively tapping controls. Screen on_input() switches
// have no case for UI_INPUT_NONE, so it's a no-op there.
static void parent_clicked_cb(lv_event_t *e)
{
    (void)e;
    ui_input_send(UI_INPUT_NONE);   // keep screensaver idle timer alive
    if (!s_overlay) return;
    if (lv_obj_has_flag(s_overlay, LV_OBJ_FLAG_HIDDEN)) overlay_show();
}

static void btn_clicked_cb(lv_event_t *e)
{
    ui_input_send(UI_INPUT_NONE);   // keep screensaver idle timer alive
    ctrl_id_t btn = (ctrl_id_t)(intptr_t)lv_event_get_user_data(e);
    // ESP_LOGI(TAG, "btn_c: %d", id);
    app_state_t *s = app_state_get();
    ui_screen_id_t scr = s->screen;

    if (scr == SCREEN_RADIO) {
        switch (btn) {
            case CTRL_VOL_UP:
            case CTRL_VOL_DN: {
                int vol = s->volume;
                vol += (btn == CTRL_VOL_UP) ? 2 : -2;
                if (vol < 0)   vol = 0;
                if (vol > 100) vol = 100;

                settings_set_volume(vol);   // → audio_player + app_state + save
                break;
            }
            case CTRL_PLAY: {
                if(s->radio_state ==  RADIO_STATE_STOPPED) {
                    radio_play_index(s->curr_index);
                    lv_label_set_text(s_play_lbl, LV_SYMBOL_STOP);
                }
                else if(s->radio_state ==  RADIO_STATE_PLAYING) {
                    radio_stop();
                    lv_label_set_text(s_play_lbl, LV_SYMBOL_PLAY);
                }
                break;
            }
            case CTRL_NEXT:
            case CTRL_PREV: {
                int curr = s->curr_index;
                curr += (btn == CTRL_NEXT) ? 1 : -1;
                radio_play_index(curr);
                break;
            }
            default: break;
        }
    }
    else if (scr == SCREEN_BT) {
        switch (btn) {
            case CTRL_VOL_UP:
            case CTRL_VOL_DN: {
                int vol = s->bt_volume;
                vol += (btn == CTRL_VOL_UP) ? 2 : -2;
                if (vol < 0)   vol = 0;
                if (vol > 100) vol = 100;

                settings_set_bt_volume(vol);   // → audio_player + app_state + save
                break;
            }
            case CTRL_PLAY: {
                break;
            }
            default: break;
        }
    }


    // keep overlay open and reset the auto-hide timer
    if (s_timer) lv_timer_reset(s_timer);
}

static void btn_long_repeat_cb(lv_event_t *e)
{
    ui_input_send(UI_INPUT_NONE);   // keep screensaver idle timer alive
    ctrl_id_t btn = (ctrl_id_t)(intptr_t)lv_event_get_user_data(e);
    // ESP_LOGI(TAG, "btn_l: %d", id);
    app_state_t *s = app_state_get();
    ui_screen_id_t scr = s->screen;

    if (scr == SCREEN_RADIO) {
        switch (btn) {
            case CTRL_VOL_UP:
            case CTRL_VOL_DN: {
                int vol = s->volume;
                vol += (btn == CTRL_VOL_UP) ? 2 : -2;
                if (vol < 0)   vol = 0;
                if (vol > 100) vol = 100;

                settings_set_volume(vol);   // → audio_player + app_state + save
                break;
            }
            default: break;
        }
    }
    else if (scr == SCREEN_BT) {
        switch (btn) {
            case CTRL_VOL_UP:
            case CTRL_VOL_DN: {
                int vol = s->bt_volume;
                vol += (btn == CTRL_VOL_UP) ? 2 : -2;
                if (vol < 0)   vol = 0;
                if (vol > 100) vol = 100;

                settings_set_bt_volume(vol);   // → audio_player + app_state + save
                break;
            }
            default: break;
        }
    }


    // keep overlay open and reset the auto-hide timer
    if (s_timer) lv_timer_reset(s_timer);
}

// label_scale: 256 == 100%; lower values shrink the symbol around its center
// so it stays inside the circle (useful for visually-heavier glyphs like ▶).
static lv_obj_t *make_btn(lv_obj_t *parent, const char *symbol, const intptr_t id,
                          int dx, int dy, int size, lv_color_t bg, int label_scale,
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
    if (label_scale != 256) {
        lv_obj_set_style_transform_pivot_x(lbl, lv_pct(50), LV_PART_MAIN);
        lv_obj_set_style_transform_pivot_y(lbl, lv_pct(50), LV_PART_MAIN);
        lv_obj_set_style_transform_scale(lbl, label_scale, LV_PART_MAIN);
    }

    if (evt_mask & CTRL_EVT_SHORT) {
        lv_obj_add_event_cb(btn, btn_clicked_cb,
                            LV_EVENT_SHORT_CLICKED, (void *)id);
    }
    if (evt_mask & CTRL_EVT_REPEAT) {
        lv_obj_add_event_cb(btn, btn_long_repeat_cb,
                            LV_EVENT_LONG_PRESSED_REPEAT, (void *)id);
    }
    return btn;
}

void controls_overlay_create(lv_obj_t *parent)
{
    if (!parent || s_overlay) return;
    s_parent = parent;
    const ui_theme_colors_t *th = theme_get();

    // Make screen background tap-aware (label children won't bubble by
    // default, so taps in empty areas reach the parent screen object).
    lv_obj_add_flag(parent, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(parent, parent_clicked_cb, LV_EVENT_CLICKED, NULL);

    // Dim overlay covering the whole screen — clicking the dim area hides.
    s_overlay = lv_obj_create(parent);
    lv_obj_remove_style_all(s_overlay);
    lv_obj_set_size(s_overlay, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_set_pos(s_overlay, 0, 0);
    lv_obj_set_style_bg_color(s_overlay, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_overlay, LV_OPA_60, LV_PART_MAIN);
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_CLICKABLE);  // absorb taps on the dim area
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);

    // Button diameter & cross step proportional to the shorter screen side
    int side = (DISPLAY_WIDTH < DISPLAY_HEIGHT) ? DISPLAY_WIDTH : DISPLAY_HEIGHT;
    int sz   = side / 3;
    if (sz < 56)  sz = 56;
    if (sz > 110) sz = 110;
    int step = sz + sz / 40;   // center-to-center distance for cross arms

    lv_color_t btn_bg     = lv_color_hex(th->bg_secondary);
    lv_color_t center_bg  = lv_color_hex(th->accent);

    make_btn(s_overlay, LV_SYMBOL_PLUS,  CTRL_VOL_UP, 0,    -step, sz, btn_bg,    256,
        CTRL_EVT_SHORT | CTRL_EVT_REPEAT);
    make_btn(s_overlay, LV_SYMBOL_MINUS, CTRL_VOL_DN, 0,     step, sz, btn_bg,    256,
        CTRL_EVT_SHORT | CTRL_EVT_REPEAT);
    make_btn(s_overlay, LV_SYMBOL_PREV,  CTRL_PREV,  -step,  0,    sz, btn_bg,    256,
        CTRL_EVT_SHORT);
    make_btn(s_overlay, LV_SYMBOL_NEXT,  CTRL_NEXT,   step,  0,    sz, btn_bg,    256,
        CTRL_EVT_SHORT);

    bool playing = (app_state_get()->radio_state == RADIO_STATE_PLAYING);
    lv_obj_t *play_btn = make_btn(s_overlay, playing ? LV_SYMBOL_STOP : LV_SYMBOL_PLAY,  CTRL_PLAY,   0,     0,    sz, center_bg, 180,
        CTRL_EVT_SHORT);
    s_play_lbl = lv_obj_get_child(play_btn, 0);

    ESP_LOGI(TAG, "Created (btn=%dpx, step=%dpx)", sz, step);
}

void controls_overlay_destroy(void)
{
    if (s_timer)   { lv_timer_del(s_timer); s_timer = NULL; }
    // Parent is owned by the screen; LVGL frees it. Drop our refs only.
    s_play_lbl = NULL;
    s_overlay = NULL;
    s_parent  = NULL;
}
