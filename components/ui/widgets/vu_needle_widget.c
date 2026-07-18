#include "vu_needle_widget.h"
#include "audio_levels.h"   // per-channel RMS tapped from the DSP element
#include "app_state.h"      // volume % scales the needle swing
#include "theme.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "VU_NEEDLE";

// ── Tunables ────────────────────────────────────────────────────────────────
#define NEEDLE_TICK_MS   50        // refresh period (~20 fps, same as the bar VU)
#define NEEDLE_SWEEP_DEG 90.0f     // full deflection arc: -45° (rest) … +45°
// AGC: same idea as the bar VU — the needle rides a slowly-tracked reference
// level instead of a fixed dB window, so loud and quiet stations both use the
// full arc. Values differ from the bar VU because this works on RMS dBFS, not
// FFT peak power.
// Full-band RMS on loudness-compressed broadcast moves only a few dB, far less
// than the bar VU's per-band FFT peaks — so the window here must be much
// narrower than the bar VU's 24 dB or the needle barely leaves its resting arc.
#define NEEDLE_AGC_RANGE_DB 14.0f  // window below the reference mapped to 0..1
                                   // (the swing knob: smaller → more motion/dB)
#define NEEDLE_AGC_ATTACK   0.5f   // ref rise smoothing per tick
#define NEEDLE_AGC_FALL_DB  0.1f   // ref drop per tick (2 dB/s @ 20 fps)
#define NEEDLE_AGC_REF_MIN  (-40.0f) // dBFS floor: don't chase idle hiss/silence
// Ballistics: quicker than a textbook 300 ms VU movement so beats visibly kick
// the needle; attack leads decay so it jumps up and falls back a touch lazier.
#define NEEDLE_ATTACK    0.45f     // smoothing per tick when rising
#define NEEDLE_DECAY     0.25f     // smoothing per tick when falling
// >1 expands the TOP of the range (opposite of the bar VU's 0.8): the AGC ref
// rides the recent peak, so the programme sits at level 0.8-1.0 and the needle
// looked pinned; gamma 2.2 moves the operating point down to ~2/3 of the arc
// and roughly doubles the visible swing of the same few-dB programme dynamics.
#define NEEDLE_GAMMA     2.2f

typedef struct {
    lv_obj_t *cont;      // NULL when this side is hidden
    int       w, h;
    int       piv_x, piv_y;  // pivot, container-relative (bottom-centre)
    int       len;           // needle length in px
    int       tip_x, tip_y;  // last drawn tip, container-relative
    float     lvl;           // smoothed level 0..1
} needle_meter_t;

static needle_meter_t s_m[2];         // [0]=left, [1]=right channel
static lv_timer_t    *s_timer = NULL;
static bool           s_created = false;
static float          s_agc_ref = NEEDLE_AGC_REF_MIN; // shared L/R reference (dBFS)
static uint32_t       s_last_count = 0;               // stall (pause) detection

static void tip_for_level(const needle_meter_t *m, float lvl, int *tx, int *ty)
{
    float a = (lvl - 0.5f) * NEEDLE_SWEEP_DEG * ((float)M_PI / 180.0f);
    *tx = m->piv_x + (int)(sinf(a) * (float)m->len + 0.5f);
    *ty = m->piv_y - (int)(cosf(a) * (float)m->len + 0.5f);
}

// Custom draw: just the needle line and a small pivot cap, painted over the
// (wallpaper) background. The optional frame is a plain LVGL border style.
static void needle_draw_cb(lv_event_t *e)
{
    needle_meter_t *m = lv_event_get_user_data(e);
    lv_layer_t *layer = lv_event_get_layer(e);
    const ui_theme_colors_t *th = theme_get();

    lv_area_t a;
    lv_obj_get_coords(m->cont, &a);

    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init(&line);
    line.color = lv_color_hex(th->vu_bar);
    line.width = 2;
    line.round_start = 1;
    line.round_end = 1;
    line.p1.x = a.x1 + m->piv_x;
    line.p1.y = a.y1 + m->piv_y;
    line.p2.x = a.x1 + m->tip_x;
    line.p2.y = a.y1 + m->tip_y;
    lv_draw_line(layer, &line);

    lv_draw_rect_dsc_t cap;
    lv_draw_rect_dsc_init(&cap);
    cap.bg_color = lv_color_hex(th->vu_bar);
    cap.bg_opa   = LV_OPA_COVER;
    cap.radius   = LV_RADIUS_CIRCLE;
    lv_area_t dot = {
        .x1 = a.x1 + m->piv_x - 3, .y1 = a.y1 + m->piv_y - 3,
        .x2 = a.x1 + m->piv_x + 3, .y2 = a.y1 + m->piv_y + 3,
    };
    lv_draw_rect(layer, &cap, &dot);
}

// Move one needle to its smoothed level and invalidate only the union bbox of
// the old and new needle (delta) — the meter body never repaints, so a large
// meter over a wallpaper stays cheap: cost follows needle motion.
static void meter_render(needle_meter_t *m)
{
    int tx, ty;
    tip_for_level(m, m->lvl, &tx, &ty);
    if (tx == m->tip_x && ty == m->tip_y) return;

    lv_area_t a;
    lv_obj_get_coords(m->cont, &a);

    int x_lo = m->piv_x, x_hi = m->piv_x;
    int y_lo = m->piv_y, y_hi = m->piv_y;
    if (m->tip_x < x_lo) x_lo = m->tip_x;
    if (m->tip_x > x_hi) x_hi = m->tip_x;
    if (tx < x_lo)       x_lo = tx;
    if (tx > x_hi)       x_hi = tx;
    if (m->tip_y < y_lo) y_lo = m->tip_y;
    if (m->tip_y > y_hi) y_hi = m->tip_y;
    if (ty < y_lo)       y_lo = ty;
    if (ty > y_hi)       y_hi = ty;

    m->tip_x = tx;
    m->tip_y = ty;

    // ±4 px margin covers the 2 px line width, rounded ends and AA fringe.
    lv_area_t inv = {
        .x1 = a.x1 + x_lo - 4, .y1 = a.y1 + y_lo - 4,
        .x2 = a.x1 + x_hi + 4, .y2 = a.y1 + y_hi + 4,
    };
    lv_obj_invalidate_area(m->cont, &inv);
}

static void smooth_to(needle_meter_t *m, float target)
{
    float k = (target > m->lvl) ? NEEDLE_ATTACK : NEEDLE_DECAY;
    m->lvl += (target - m->lvl) * k;
}

// LVGL-task timer: RMS → dB → AGC window → ballistics → needle angle.
static void tick_cb(lv_timer_t *t)
{
    (void)t;
    float target[2] = { 0.0f, 0.0f };

    // No new audio since last tick (paused / stopped): needles fall to rest.
    uint32_t count = audio_levels_count();
    if (count != s_last_count) {
        s_last_count = count;

        float rms_l, rms_r;
        audio_levels_get_stereo(&rms_l, &rms_r);
        float db[2] = {
            20.0f * log10f(rms_l + 1e-6f),
            20.0f * log10f(rms_r + 1e-6f),
        };

        float frame_max = db[0] > db[1] ? db[0] : db[1];
        if (frame_max > s_agc_ref) s_agc_ref += (frame_max - s_agc_ref) * NEEDLE_AGC_ATTACK;
        else                       s_agc_ref -= NEEDLE_AGC_FALL_DB;
        if (s_agc_ref < NEEDLE_AGC_REF_MIN) s_agc_ref = NEEDLE_AGC_REF_MIN;

        // The DSP tap is deliberately PRE-volume (the AGC window tracks the
        // music, not the listening level — and the ^4 volume taper would park
        // the needle in real dB). Scale the swing by the volume instead, so the
        // needle visibly follows the knob like a real amplifier's meter. Square
        // root, not linear: linear crushed the swing at quiet listening levels
        // (25% volume left a quarter of the arc); sqrt keeps half the arc there
        // while still dropping the needle to rest as the knob approaches zero.
        float vol = (float)app_state_get()->volume * 0.01f;
        if (vol < 0.0f) vol = 0.0f; else if (vol > 1.0f) vol = 1.0f;
        float vol_scale = sqrtf(vol);

        for (int i = 0; i < 2; i++) {
            float lvl = (db[i] - (s_agc_ref - NEEDLE_AGC_RANGE_DB)) / NEEDLE_AGC_RANGE_DB;
            if (lvl < 0.0f) lvl = 0.0f; else if (lvl > 1.0f) lvl = 1.0f;
            target[i] = powf(lvl, NEEDLE_GAMMA) * vol_scale;
        }
    }

    for (int i = 0; i < 2; i++) {
        if (!s_m[i].cont) continue;
        smooth_to(&s_m[i], target[i]);
        meter_render(&s_m[i]);
    }
}

static void meter_create(needle_meter_t *m, lv_obj_t *parent,
                         int16_t x, int16_t y, int16_t w, int16_t h, bool frame)
{
    const ui_theme_colors_t *th = theme_get();

    // Guard degenerate geometry (fresh profile fields can be 0).
    if (w < 20) w = 20;
    if (h < 20) h = 20;

    m->w = w;
    m->h = h;
    m->piv_x = w / 2;
    m->piv_y = h - 3;
    // Longest needle that stays inside the rect at full ±45° deflection.
    int len_h = m->piv_y - 3;
    int len_w = (int)((float)(m->piv_x - 3) / sinf(NEEDLE_SWEEP_DEG * 0.5f *
                                                  ((float)M_PI / 180.0f)));
    m->len = len_h < len_w ? len_h : len_w;
    if (m->len < 4) m->len = 4;
    m->lvl = 0.0f;

    m->cont = lv_obj_create(parent);
    lv_obj_remove_style_all(m->cont);
    lv_obj_set_size(m->cont, w, h);
    lv_obj_set_pos(m->cont, x, y);
    lv_obj_clear_flag(m->cont, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    // Background stays transparent by design — the meter face is the wallpaper.
    if (frame) {
        lv_obj_set_style_border_color(m->cont, lv_color_hex(th->vu_bg), 0);
        lv_obj_set_style_border_width(m->cont, 1, 0);
        lv_obj_set_style_border_opa(m->cont, LV_OPA_COVER, 0);
    }
    lv_obj_add_event_cb(m->cont, needle_draw_cb, LV_EVENT_DRAW_POST, m);

    tip_for_level(m, 0.0f, &m->tip_x, &m->tip_y);
}

void vu_needle_widget_create(lv_obj_t *parent,
                             bool show_l, int16_t l_x, int16_t l_y, int16_t l_w, int16_t l_h,
                             bool show_r, int16_t r_x, int16_t r_y, int16_t r_w, int16_t r_h,
                             bool frame)
{
    if (s_created) return;
    if (!show_l && !show_r) return;
    s_created = true;

    s_m[0] = (needle_meter_t){ 0 };
    s_m[1] = (needle_meter_t){ 0 };
    if (show_l) meter_create(&s_m[0], parent, l_x, l_y, l_w, l_h, frame);
    if (show_r) meter_create(&s_m[1], parent, r_x, r_y, r_w, r_h, frame);

    s_agc_ref = NEEDLE_AGC_REF_MIN;   // re-anchor to the new programme on recreate
    s_last_count = audio_levels_count();
    s_timer = lv_timer_create(tick_cb, NEEDLE_TICK_MS, NULL);
    ESP_LOGI(TAG, "Created (L:%d R:%d)", show_l, show_r);
}

void vu_needle_widget_destroy(void)
{
    if (!s_created) return;
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    for (int i = 0; i < 2; i++) {
        if (s_m[i].cont) { lv_obj_delete(s_m[i].cont); s_m[i].cont = NULL; }
    }
    s_created = false;
    ESP_LOGI(TAG, "Destroyed");
}

void vu_needle_widget_apply_theme(void)
{
    if (!s_created) return;
    const ui_theme_colors_t *th = theme_get();
    for (int i = 0; i < 2; i++) {
        if (!s_m[i].cont) continue;
        lv_obj_set_style_border_color(s_m[i].cont, lv_color_hex(th->vu_bg), 0);
        lv_obj_invalidate(s_m[i].cont);   // needle colour is read fresh in the draw cb
    }
}
