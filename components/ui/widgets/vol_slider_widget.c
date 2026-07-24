#include "vol_slider_widget.h"
#include "app_state.h"
#include "settings.h"
#include "audio_engine.h"
#include "theme.h"
#include "lv_bin_image.h"

static lv_obj_t       *s_slider    = NULL;
static bool            s_bt        = false;
static bool            s_knob_only = false;
static int             s_vol_max   = 100;   // slider full travel maps to 0..s_vol_max %

// Map slider travel (0..100) to the effective output volume, and back. The
// slider's LVGL range stays 0..100 (so the knob geometry is unchanged); only
// the value written to / read from the audio path is scaled by s_vol_max.
static inline int travel_to_vol(int pos) { return (pos * s_vol_max + 50) / 100; }
static inline int vol_to_travel(int vol)
{
    int pos = (vol * 100 + s_vol_max / 2) / s_vol_max;
    return pos < 0 ? 0 : (pos > 100 ? 100 : pos);
}

// Knob artwork is drawn by a SEPARATE lv_image overlaid on the slider, not as
// the slider knob's bg_image. A plain lv_image blits directly (like the
// wallpaper) and scales to any size cleanly, while a bg_image on the knob part
// is tied to the slider's own draw path. The slider's own knob is made
// invisible; this image is positioned to track the value.
static lv_obj_t       *s_knob_img  = NULL;
static lv_image_dsc_t *s_knob_dsc  = NULL;   // scaled artwork, freed on destroy
static int             s_x, s_y, s_w, s_h;   // slider box (final, post-normalise)
static int             s_kw, s_kh;           // knob image size
static bool            s_vertical  = false;

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

// Move the knob image to match the current slider value. No-op without an image.
static void position_knob(void)
{
    if (!s_knob_img || !s_slider) return;
    int v = (int)lv_slider_get_value(s_slider);   // 0..100
    int travel_x = s_w - s_kw; if (travel_x < 0) travel_x = 0;
    int travel_y = s_h - s_kh; if (travel_y < 0) travel_y = 0;
    int kx, ky;
    if (s_vertical) {
        kx = s_x + (s_w - s_kw) / 2;
        ky = s_y + travel_y * (100 - v) / 100;   // v=100 → top
    } else {
        kx = s_x + travel_x * v / 100;           // v=0 → left
        ky = s_y + (s_h - s_kh) / 2;
    }
    lv_obj_set_pos(s_knob_img, kx, ky);
}

static void value_changed_cb(lv_event_t *e)
{
    position_knob();    // follow the knob live, on the main and BT channels alike
    if (s_bt) return;   // BT applies on release only — see header
    lv_obj_t *sl = lv_event_get_target(e);
    audio_engine_set_volume(travel_to_vol((int)lv_slider_get_value(sl)));
}

static void released_cb(lv_event_t *e)
{
    lv_obj_t *sl = lv_event_get_target(e);
    int vol = travel_to_vol((int)lv_slider_get_value(sl));
    if (s_bt) settings_set_bt_volume(vol);   // → bt_set_volume + app_state + save
    else      settings_set_volume(vol);      // → audio_engine + app_state + save
}

// Load the knob .bin and build the tracking image. The slider's thickness (cross
// axis after normalisation: h for horizontal, w for vertical) sets the knob size;
// the other axis follows the image's aspect ratio. On any failure the slider
// keeps its plain themed knob.
static void build_knob_image(lv_obj_t *parent, const char *knob_image, int w, int h)
{
    if (!knob_image || !knob_image[0]) return;

    s_knob_dsc = lv_bin_image_load(knob_image, 0, 0);
    if (!s_knob_dsc) return;

    const int iw = s_knob_dsc->header.w, ih = s_knob_dsc->header.h;
    if (s_vertical) {                 // cross axis = width
        s_kw = w;
        s_kh = ih * s_kw / iw;
    } else {                          // cross axis = height
        s_kh = h;
        s_kw = iw * s_kh / ih;
    }
    if (s_kw < 1) s_kw = 1;
    if (s_kh < 1) s_kh = 1;

    s_knob_dsc = lv_bin_image_scale(s_knob_dsc, s_kw, s_kh);   // consumes the native dsc
    if (!s_knob_dsc) return;

    // Hide the slider's own knob — the image is the knob now.
    lv_obj_set_style_bg_opa      (s_slider, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_border_width(s_slider, 0, LV_PART_KNOB);
    lv_obj_set_style_shadow_width(s_slider, 0, LV_PART_KNOB);
    lv_obj_set_style_outline_width(s_slider, 0, LV_PART_KNOB);

    // Sibling above the slider; not clickable so touches fall through to the
    // slider underneath (which still owns the drag).
    s_knob_img = lv_image_create(parent);
    lv_image_set_src(s_knob_img, s_knob_dsc);
    lv_obj_remove_flag(s_knob_img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(s_knob_img, s_kw, s_kh);
    position_knob();
}

void vol_slider_widget_create(lv_obj_t *parent, int16_t x, int16_t y,
                              int16_t w, int16_t h, bool vertical,
                              bool knob_only, bool bt, const char *knob_image,
                              int vol_max)
{
    if (!parent || s_slider) return;
    s_bt        = bt;
    s_knob_only = knob_only;
    s_vol_max   = (vol_max < 1 || vol_max > 100) ? 100 : vol_max;   // 0/unset → no scaling

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
    // A drag on the slider must not double as a screen swipe: gestures bubble to
    // the screen-level handler (ui_manager) by default, so a vertical drag would
    // also fire swipe up/down navigation. Stop the bubble at the slider.
    lv_obj_remove_flag(s_slider, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_set_ext_click_area(s_slider, 8);   // thin tracks stay grabbable
    lv_obj_add_event_cb(s_slider, value_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(s_slider, released_cb, LV_EVENT_RELEASED, NULL);

    apply_colors();

    // The default theme gives the slider CIRCLE-radius parts. On a large slider
    // the knob's radius (up to thickness/2) becomes a big anti-aliased circle
    // mask that hangs the SW renderer here — the render freeze was size-
    // thresholded while memory stayed healthy, and squaring the corners fixed it
    // (verified: 251x261 renders fine, LVGL task load normal). Keep radius 0.
    lv_obj_set_style_radius (s_slider, 0, LV_PART_MAIN);
    lv_obj_set_style_radius (s_slider, 0, LV_PART_INDICATOR);
    lv_obj_set_style_radius (s_slider, 0, LV_PART_KNOB);
    lv_obj_set_style_pad_all(s_slider, 0, LV_PART_KNOB);

    s_x = x; s_y = y; s_w = w; s_h = h; s_vertical = vertical;
    build_knob_image(parent, knob_image, w, h);

    vol_slider_widget_update();
}

void vol_slider_widget_destroy(void)
{
    if (s_knob_img) {                    // delete before freeing the pixels it points at
        lv_obj_del(s_knob_img);
        s_knob_img = NULL;
    }
    if (s_slider) {
        lv_obj_del(s_slider);
        s_slider = NULL;
    }
    if (s_knob_dsc) {
        lv_bin_image_free(s_knob_dsc);
        s_knob_dsc = NULL;
    }
}

void vol_slider_widget_update(void)
{
    if (!s_slider) return;
    if (lv_obj_has_state(s_slider, LV_STATE_PRESSED)) return;   // mid-drag
    app_state_t *s = app_state_get();
    lv_slider_set_value(s_slider, vol_to_travel(s_bt ? s->bt_volume : s->volume), LV_ANIM_OFF);
    position_knob();   // keep the artwork in sync with encoder/WS/Android changes
}

void vol_slider_widget_apply_theme(void)
{
    apply_colors();
}
