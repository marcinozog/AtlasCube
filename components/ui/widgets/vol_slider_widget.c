#include "vol_slider_widget.h"
#include "app_state.h"
#include "settings.h"
#include "audio_engine.h"
#include "theme.h"

static lv_obj_t *s_slider    = NULL;
static bool      s_bt        = false;
static bool      s_knob_only = false;

static void apply_colors(void)
{
    if (!s_slider) return;
    const ui_theme_colors_t *th = theme_get();
    uint32_t fill = s_bt ? th->bt_brand : th->accent;
    lv_opa_t track_opa = s_knob_only ? LV_OPA_TRANSP : LV_OPA_COVER;
    lv_obj_set_style_bg_color(s_slider, lv_color_hex(th->text_muted), LV_PART_MAIN);
    lv_obj_set_style_bg_opa  (s_slider, track_opa, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_slider, lv_color_hex(fill), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa  (s_slider, track_opa, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_slider, lv_color_hex(fill), LV_PART_KNOB);
}

static void value_changed_cb(lv_event_t *e)
{
    if (s_bt) return;   // BT applies on release only — see header
    lv_obj_t *sl = lv_event_get_target(e);
    audio_engine_set_volume((int)lv_slider_get_value(sl));
}

static void released_cb(lv_event_t *e)
{
    lv_obj_t *sl = lv_event_get_target(e);
    int vol = (int)lv_slider_get_value(sl);
    if (s_bt) settings_set_bt_volume(vol);   // → bt_set_volume + app_state + save
    else      settings_set_volume(vol);      // → audio_engine + app_state + save
}

void vol_slider_widget_create(lv_obj_t *parent, int16_t x, int16_t y,
                              int16_t w, int16_t h, bool vertical,
                              bool knob_only, bool bt)
{
    if (!parent || s_slider) return;
    s_bt        = bt;
    s_knob_only = knob_only;

    /* The box must agree with the chosen orientation: LVGL 9.2 sliders take
       the drag axis from w >= h regardless of lv_bar_set_orientation, so a
       contradicting box is swapped (and a square nudged 1 px). */
    if (vertical != (h > w)) {
        int16_t t = w; w = h; h = t;
    }
    if (vertical && w >= h) w = h - 1;

    s_slider = lv_slider_create(parent);
    lv_obj_set_pos(s_slider, x, y);
    lv_obj_set_size(s_slider, w, h);
    lv_bar_set_orientation(s_slider, vertical ? LV_BAR_ORIENTATION_VERTICAL
                                              : LV_BAR_ORIENTATION_HORIZONTAL);
    lv_slider_set_range(s_slider, 0, 100);
    /* Keep press ownership on the slider even if the finger drifts outside
       its bounds during drag — otherwise LV_EVENT_RELEASED is routed to
       whichever widget happens to be under the finger on release. */
    lv_obj_add_flag(s_slider, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_set_ext_click_area(s_slider, 8);   // thin tracks stay grabbable
    lv_obj_add_event_cb(s_slider, value_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(s_slider, released_cb, LV_EVENT_RELEASED, NULL);

    apply_colors();
    vol_slider_widget_update();
}

void vol_slider_widget_destroy(void)
{
    if (s_slider) {
        lv_obj_del(s_slider);
        s_slider = NULL;
    }
}

void vol_slider_widget_update(void)
{
    if (!s_slider) return;
    if (lv_obj_has_state(s_slider, LV_STATE_PRESSED)) return;   // mid-drag
    app_state_t *s = app_state_get();
    lv_slider_set_value(s_slider, s_bt ? s->bt_volume : s->volume, LV_ANIM_OFF);
}

void vol_slider_widget_apply_theme(void)
{
    apply_colors();
}
