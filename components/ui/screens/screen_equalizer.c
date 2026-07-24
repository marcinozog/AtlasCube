#include "screen_equalizer.h"
#include "app_state.h"
#include "settings.h"
#include "theme.h"
#include "ui_profile.h"
#include "fonts/ui_fonts.h"
#include "ui_screen.h"
#include "ui_events.h"
#include "ui_manager.h"
#include "lv_bin_image.h"
#include "lvgl.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "SCR_EQ";

#define EQ_BANDS     10
#define EQ_GAIN_MIN  -13
#define EQ_GAIN_MAX    6

static const char *FREQ_LABELS[EQ_BANDS] = {
    "31", "62", "125", "250", "500", "1k", "2k", "4k", "8k", "16k"
};

static lv_obj_t *s_root          = NULL;
static lv_obj_t *s_title         = NULL;
static lv_obj_t *s_info          = NULL;
static lv_obj_t *s_hint          = NULL;
static lv_obj_t *s_band_cont     = NULL;
static lv_obj_t *s_sliders[EQ_BANDS] = {0};
static lv_obj_t *s_freq_labels[EQ_BANDS] = {0};

static int s_focus = 0;                   // index of the active band
static int s_gains[EQ_BANDS] = {0};       // local copy — kept in sync continuously
static ui_screen_id_t s_return = SCREEN_SETTINGS;   // where exit goes (see header)

void screen_equalizer_set_return(ui_screen_id_t scr) { s_return = scr; }

// Optional knob artwork (ui_profile.eq_knob_image): one .bin decoded and scaled
// ONCE, then shown as a separate lv_image per band tracking that band's value —
// the same overlay trick as the volume slider. All 10 images share the single
// scaled descriptor; the inactive bands are dimmed so the focused one stands out.
#define EQ_KNOB_DIM_OPA   LV_OPA_40        // inactive band knobs
static lv_image_dsc_t *s_knob_dsc = NULL;  // scaled artwork, freed on destroy
static lv_obj_t       *s_knob_imgs[EQ_BANDS] = {0};
static int             s_kw, s_kh;         // scaled knob size (px)

/* ── helpers ────────────────────────────────────────────────────────────── */

static void update_info_label(void)
{
    if (!s_info) return;
    char buf[32];
    int g = s_gains[s_focus];
    snprintf(buf, sizeof(buf), "%s Hz: %+d dB", FREQ_LABELS[s_focus], g);
    lv_label_set_text(s_info, buf);
}

// Move band `idx`'s knob image to match its gain. No-op unless artwork loaded.
// Coordinates are relative to s_band_cont (the images are its children).
static void position_eq_knob(int idx)
{
    if (!s_knob_imgs[idx]) return;
    const ui_profile_t *p = ui_profile_get();
    int col_x    = idx * p->eq_band_w;
    int travel_y = p->eq_slider_h - s_kh; if (travel_y < 0) travel_y = 0;
    int span     = EQ_GAIN_MAX - EQ_GAIN_MIN;                 // 19
    int ky       = travel_y * (EQ_GAIN_MAX - s_gains[idx]) / span;   // gain=max → top
    int kx       = col_x + (p->eq_band_w - s_kw) / 2;
    lv_obj_set_pos(s_knob_imgs[idx], kx, ky);
}

static void update_slider_visual(int idx)
{
    if (idx < 0 || idx >= EQ_BANDS || !s_sliders[idx]) return;
    lv_slider_set_value(s_sliders[idx], s_gains[idx], LV_ANIM_OFF);
    position_eq_knob(idx);
}

static void slider_touch_cb(lv_event_t *e); /* fwd */

/* The round knob (diameter ≈ slider width) overflows the track ends by its
   radius at min/max. OVERFLOW_VISIBLE alone isn't enough: LVGL clips children
   to the container bounds expanded only by the container's own ext_draw_size,
   which defaults to 0. Reserve the knob radius here so the knob isn't cut. */
static void band_cont_ext_draw_cb(lv_event_t *e)
{
    const ui_profile_t *p = ui_profile_get();
    /* knob diameter = slider width + the theme's knob padding (the knob is much
       wider than the thin track), so it overflows the track ends by half of
       that. Pull the padding off a live slider rather than guessing. */
    int32_t pad = 0;
    if (s_sliders[0]) {
        int32_t pt = lv_obj_get_style_pad_top(s_sliders[0], LV_PART_KNOB);
        int32_t pb = lv_obj_get_style_pad_bottom(s_sliders[0], LV_PART_KNOB);
        pad = LV_MAX(pt, pb);
    }
    lv_event_set_ext_draw_size(e, p->eq_slider_w / 2 + pad + 2);
}

static void update_focus_visuals(void)
{
    const ui_theme_colors_t *th = theme_get();
    for (int i = 0; i < EQ_BANDS; i++) {
        if (!s_sliders[i]) continue;
        uint32_t c = (i == s_focus) ? th->accent : th->text_muted;
        lv_obj_set_style_bg_color(s_sliders[i], lv_color_hex(c), LV_PART_INDICATOR);
        // The native-knob outline highlight would draw over the artwork (the knob
        // is hidden but its border still paints) — with images, dim instead.
        int knob_border = (s_knob_imgs[i]) ? 0 : ((i == s_focus) ? 2 : 0);
        lv_obj_set_style_border_width(s_sliders[i], knob_border, LV_PART_KNOB);
        lv_obj_set_style_border_color(s_sliders[i], lv_color_hex(th->accent), LV_PART_KNOB);

        // With knob artwork the native knob is hidden, so highlight focus by
        // dimming every inactive band's image instead of an outline.
        if (s_knob_imgs[i])
            lv_obj_set_style_image_opa(s_knob_imgs[i],
                (i == s_focus) ? LV_OPA_COVER : EQ_KNOB_DIM_OPA, LV_PART_MAIN);

        if (s_freq_labels[i]) {
            uint32_t tc = (i == s_focus) ? th->accent : th->text_muted;
            lv_obj_set_style_text_color(s_freq_labels[i], lv_color_hex(tc), LV_PART_MAIN);
        }
    }
}

// Load ui_profile.eq_knob_image once (cross axis = slider width; the other axis
// follows the image's aspect ratio), then overlay one lv_image per band on
// s_band_cont and hide the sliders' native knobs. All bands share the single
// scaled descriptor. On any failure the plain themed knobs stay.
static void build_eq_knobs(void)
{
    const ui_profile_t *p = ui_profile_get();
    if (!p->eq_knob_image[0] || !s_band_cont) return;

    s_knob_dsc = lv_bin_image_load(p->eq_knob_image, 0, 0);
    if (!s_knob_dsc) return;

    const int iw = s_knob_dsc->header.w, ih = s_knob_dsc->header.h;
    s_kw = p->eq_slider_w;                        // cross axis of a vertical slider
    s_kh = (iw > 0) ? ih * s_kw / iw : s_kw;
    if (s_kw < 1) s_kw = 1;
    if (s_kh < 1) s_kh = 1;

    s_knob_dsc = lv_bin_image_scale(s_knob_dsc, s_kw, s_kh);   // consumes native dsc
    if (!s_knob_dsc) return;

    for (int i = 0; i < EQ_BANDS; i++) {
        if (!s_sliders[i]) continue;
        // Hide the slider's own knob — the image is the knob now.
        lv_obj_set_style_bg_opa      (s_sliders[i], LV_OPA_TRANSP, LV_PART_KNOB);
        lv_obj_set_style_border_width(s_sliders[i], 0, LV_PART_KNOB);

        // Sibling above the sliders; not clickable so touches fall through to the
        // slider underneath (which still owns the drag).
        lv_obj_t *img = lv_image_create(s_band_cont);
        lv_image_set_src(img, s_knob_dsc);
        lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_size(img, s_kw, s_kh);
        s_knob_imgs[i] = img;
        position_eq_knob(i);
    }
}

/* ── lifecycle ──────────────────────────────────────────────────────────── */

static void eq_create(lv_obj_t *parent)
{
    s_root = parent;
    const ui_theme_colors_t *th = theme_get();
    const ui_profile_t      *p  = ui_profile_get();

    lv_obj_set_style_bg_color(parent, lv_color_hex(th->bg_primary), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, LV_PART_MAIN);

    /* fetch current bands from app_state */
    memcpy(s_gains, app_state_get()->eq, sizeof(s_gains));
    s_focus = 0;

    /* title */
    int title_h = lv_font_get_line_height(p->eq_title_font);
    if (title_h > 0 && DISPLAY_HEIGHT > 80) {
        s_title = lv_label_create(parent);
        lv_label_set_text(s_title, "Equalizer");
        lv_obj_set_style_text_font(s_title, p->eq_title_font, LV_PART_MAIN);
        lv_obj_set_style_text_color(s_title, lv_color_hex(th->accent), LV_PART_MAIN);
        lv_obj_align(s_title, LV_ALIGN_TOP_MID, 0, p->eq_title_y);
    }

    /* info — active band + dB */
    s_info = lv_label_create(parent);
    lv_obj_set_style_text_font(s_info, p->eq_info_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_info, lv_color_hex(th->text_primary), LV_PART_MAIN);
    lv_obj_align(s_info, LV_ALIGN_TOP_MID, 0, p->eq_info_y);

    /* bands container — centered horizontally */
    int total_w = p->eq_band_w * EQ_BANDS;
    s_band_cont = lv_obj_create(parent);
    lv_obj_set_size(s_band_cont, total_w, p->eq_slider_h + lv_font_get_line_height(p->eq_freq_font) + 6);
    lv_obj_align(s_band_cont, LV_ALIGN_TOP_MID, 0, p->eq_band_area_y);
    lv_obj_set_style_bg_opa(s_band_cont, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_band_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_band_cont, 0, LV_PART_MAIN);
    lv_obj_clear_flag(s_band_cont, LV_OBJ_FLAG_SCROLLABLE);
    /* let the round knob overflow past the container edges at min/max,
       otherwise its outer half gets clipped to the container bounds */
    lv_obj_add_flag(s_band_cont, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_add_event_cb(s_band_cont, band_cont_ext_draw_cb,
                        LV_EVENT_REFR_EXT_DRAW_SIZE, NULL);

    /* suwaki + etykiety frequency */
    for (int i = 0; i < EQ_BANDS; i++) {
        int col_x = i * p->eq_band_w;

        lv_obj_t *sl = lv_slider_create(s_band_cont);
        /* vertical orientation — in LVGL it follows from dimensions (h > w) */
        lv_obj_set_size(sl, p->eq_slider_w, p->eq_slider_h);
        lv_obj_set_pos(sl, col_x + (p->eq_band_w - p->eq_slider_w) / 2, 0);
        lv_slider_set_range(sl, EQ_GAIN_MIN, EQ_GAIN_MAX);
        lv_slider_set_value(sl, s_gains[i], LV_ANIM_OFF);
        lv_obj_set_style_bg_color(sl, lv_color_hex(th->text_muted), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(sl, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_bg_color(sl, lv_color_hex(th->accent), LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(sl, LV_OPA_COVER, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(sl, lv_color_hex(th->accent), LV_PART_KNOB);
        /* touch: keep the slider interactive; PRESS_LOCK keeps the drag bound
           to this slider even if the finger drifts off its (narrow) bounds */
        lv_obj_add_flag(sl, LV_OBJ_FLAG_PRESS_LOCK);
        lv_obj_add_event_cb(sl, slider_touch_cb, LV_EVENT_VALUE_CHANGED,
                            (void *)(uintptr_t)i);
        lv_obj_add_event_cb(sl, slider_touch_cb, LV_EVENT_RELEASED,
                            (void *)(uintptr_t)i);
        s_sliders[i] = sl;

        lv_obj_t *lbl = lv_label_create(s_band_cont);
        lv_label_set_text(lbl, FREQ_LABELS[i]);
        lv_obj_set_style_text_font(lbl, p->eq_freq_font, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl, lv_color_hex(th->text_muted), LV_PART_MAIN);
        lv_obj_set_width(lbl, p->eq_band_w);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_pos(lbl, col_x, p->eq_slider_h + 2);
        s_freq_labels[i] = lbl;
    }

    /* sliders now exist — recompute the container's reserved overflow room
       (the ext_draw cb reads the knob padding off s_sliders[0]) */
    lv_obj_refresh_ext_draw_size(s_band_cont);

    /* optional shared knob artwork overlaid on the bands */
    build_eq_knobs();

    /* hint */
    s_hint = lv_label_create(parent);
    lv_label_set_text(s_hint, p->settings_show_slider ? "swipe = back"
                                                      : "press=next  long=back");
    lv_obj_set_style_text_font(s_hint, p->eq_hint_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_hint, lv_color_hex(th->text_muted), LV_PART_MAIN);
    lv_obj_align(s_hint, LV_ALIGN_BOTTOM_MID, 0, p->eq_hint_y);

    update_info_label();
    update_focus_visuals();

    ESP_LOGI(TAG, "Created (focus=%d)", s_focus);
}

static void eq_destroy(void)
{
    // Delete the knob images before freeing the pixels they point at (the
    // container/sliders are torn down by ui_manager's screen clean afterwards).
    for (int i = 0; i < EQ_BANDS; i++) {
        if (s_knob_imgs[i]) { lv_obj_del(s_knob_imgs[i]); s_knob_imgs[i] = NULL; }
    }
    if (s_knob_dsc) { lv_bin_image_free(s_knob_dsc); s_knob_dsc = NULL; }

    s_root = s_title = s_info = s_hint = s_band_cont = NULL;
    for (int i = 0; i < EQ_BANDS; i++) {
        s_sliders[i] = NULL;
        s_freq_labels[i] = NULL;
    }
    ESP_LOGI(TAG, "Destroyed");
}

/* ── events & input ─────────────────────────────────────────────────────── */

static void eq_on_event(const ui_event_t *ev)
{
    if (ev->type == UI_EVT_STATE_CHANGED) {
        /* external change (e.g. from web UI) — refresh */
        memcpy(s_gains, app_state_get()->eq, sizeof(s_gains));
        for (int i = 0; i < EQ_BANDS; i++) update_slider_visual(i);
        update_info_label();
    }
}

/* touch: drag a band slider (live preview, commit on release) */
static void slider_touch_cb(lv_event_t *e)
{
    int idx = (int)(uintptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= EQ_BANDS) return;

    lv_obj_t *sl = lv_event_get_target(e);
    s_gains[idx] = (int)lv_slider_get_value(sl);
    position_eq_knob(idx);   // keep the artwork on the knob during the drag

    if (s_focus != idx) {
        s_focus = idx;
        update_focus_visuals();
    }
    update_info_label();

    /* settings_set_eq_10() writes flash — only on release, not every frame */
    if (lv_event_get_code(e) == LV_EVENT_RELEASED) {
        settings_set_eq_10(s_gains);
    }
}

static void eq_on_input(ui_input_t input)
{
    switch (input) {

        case UI_INPUT_ENCODER_CW:
        case UI_INPUT_ENCODER_CCW: {
            int delta = (input == UI_INPUT_ENCODER_CW) ? 1 : -1;
            int g = s_gains[s_focus] + delta;
            if (g < EQ_GAIN_MIN) g = EQ_GAIN_MIN;
            if (g > EQ_GAIN_MAX) g = EQ_GAIN_MAX;
            if (g == s_gains[s_focus]) break;

            s_gains[s_focus] = g;
            update_slider_visual(s_focus);
            update_info_label();
            settings_set_eq_10(s_gains);
            break;
        }

        case UI_INPUT_ENCODER_PRESS:
            s_focus = (s_focus + 1) % EQ_BANDS;
            update_focus_visuals();
            update_info_label();
            break;

        case UI_INPUT_ENCODER_LONG_PRESS:
        case UI_INPUT_SWIPE_LEFT:
        case UI_INPUT_SWIPE_RIGHT:
            /* horizontal only — vertical drags belong to the band sliders */
            ui_navigate(s_return);
            break;

        default:
            break;
    }
}

/* ── theme ──────────────────────────────────────────────────────────────── */

static void eq_apply_theme(void)
{
    if (!s_root) return;
    const ui_theme_colors_t *th = theme_get();

    lv_obj_set_style_bg_color(s_root, lv_color_hex(th->bg_primary), LV_PART_MAIN);
    if (s_title) lv_obj_set_style_text_color(s_title, lv_color_hex(th->accent), LV_PART_MAIN);
    if (s_info)  lv_obj_set_style_text_color(s_info,  lv_color_hex(th->text_primary), LV_PART_MAIN);
    if (s_hint)  lv_obj_set_style_text_color(s_hint,  lv_color_hex(th->text_muted),   LV_PART_MAIN);

    for (int i = 0; i < EQ_BANDS; i++) {
        if (s_sliders[i]) {
            lv_obj_set_style_bg_color(s_sliders[i], lv_color_hex(th->text_muted), LV_PART_MAIN);
        }
    }
    update_focus_visuals();
    lv_obj_invalidate(s_root);
}

/* ── vtable ─────────────────────────────────────────────────────────────── */

const ui_screen_t screen_equalizer = {
    .create      = eq_create,
    .destroy     = eq_destroy,
    .apply_theme = eq_apply_theme,
    .on_event    = eq_on_event,
    .on_input    = eq_on_input,
    .name        = "equalizer",
};
