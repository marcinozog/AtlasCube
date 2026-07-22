#include "vu_stereo_widget.h"
#include "audio_levels.h"   // per-channel RMS tapped from the DSP element
#include "theme.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "VU_STEREO";

// ── Tunables ────────────────────────────────────────────────────────────────
// The RMS→dB→AGC→ballistics chain is copied 1:1 from the needle VU: same audio
// tap (per-channel RMS), same "few-dB broadcast" problem, so the same window and
// gamma give a matching swing. Only the mapping of the 0..1 level differs (bar
// length here, needle angle there). Retune on hardware if bars and needles ever
// want to feel different.
#define BAR_TICK_MS      50        // refresh period (~20 fps, same as the other VUs)
#define BAR_AGC_RANGE_DB 14.0f     // window below the reference mapped to 0..1
#define BAR_AGC_ATTACK   0.5f      // ref rise smoothing per tick
#define BAR_AGC_FALL_DB  0.1f      // ref drop per tick (2 dB/s @ 20 fps)
#define BAR_AGC_REF_MIN  (-40.0f)  // dBFS floor: don't chase idle hiss/silence
#define BAR_ATTACK       0.45f     // smoothing per tick when rising
#define BAR_DECAY        0.25f     // smoothing per tick when falling
// <1 boosts perceived motion at low levels so the bar reaches the top on loud
// passages (matches the spectrum VU). The needle uses 2.2 for the OPPOSITE reason
// — it rests mid-arc, so >1 unpins it; a bar grows from zero, where >1 only ever
// shortens it, which left the bars stuck below half.
#define BAR_GAMMA        0.8f
// Peak-hold: the marker jumps to the bar level instantly, then falls back slowly
// so a floating tick separates from the bar top and reads as the recent maximum.
#define BAR_PEAK_FALL    0.015f    // peak level drop per tick (full scale in ~3 s)
#define BAR_PEAK_W       2         // peak marker thickness in px
// Colour zones, classic VU: green up to MID, orange up to HOT, red above. Zones
// are POSITIONAL (a pixel's colour depends only on its distance from the
// baseline, never on the current level) so the delta-invalidation in
// meter_render stays valid — the unchanged part of the bar never repaints.
#define BAR_ZONE_MID     0.60f     // green/orange boundary as a fraction of span
#define BAR_ZONE_HOT     0.85f     // orange/red boundary as a fraction of span
#define BAR_COLOR_LO     0x00C853  // green zone
#define BAR_COLOR_MID    0xFFB300  // orange zone
#define BAR_COLOR_HOT    0xF44336  // red zone

typedef struct {
    lv_obj_t *cont;      // NULL when this side is hidden
    int       off;       // inset from the container edge (frame width, 0 or 1)
    int       span;      // bar travel in px along the fill axis
    int       cross;     // bar size across the fill axis
    int       fill;      // last drawn bar length in px (0..span)
    int       peak;      // last drawn peak marker position in px (0..span)
    float     lvl;       // smoothed level 0..1
    float     peak_lvl;  // smoothed peak level 0..1
} bar_meter_t;

static bar_meter_t   s_m[2];        // [0]=left, [1]=right channel
static lv_timer_t   *s_timer = NULL;
static bool          s_created = false;
static bool          s_horizontal = false;   // fill left→right instead of bottom→up
static bool          s_transparent = false;  // no bg fill: bars sit on wallpaper
static bool          s_peak = false;         // draw the peak-hold marker
static bool          s_zones = false;        // colour zones; false = theme vu_bar
static float         s_agc_ref = BAR_AGC_REF_MIN; // shared L/R reference (dBFS)
static uint32_t      s_last_count = 0;            // stall (pause) detection
static media_source_t s_owner = MEDIA_SOURCE_RADIO; // source these bars belong to

// Fill the band [lo .. hi] (1-based distance from the baseline, inclusive)
// across the bar with a solid colour. Pixel at distance d sits at
// base_y - d + 1 (vertical) / base_x + d - 1 (horizontal).
static void draw_band(lv_layer_t *layer, const bar_meter_t *m,
                      int bx, int by, int base_x, int base_y,
                      int lo, int hi, uint32_t color)
{
    if (hi < lo) return;
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color = lv_color_hex(color);
    dsc.bg_opa   = LV_OPA_COVER;
    lv_area_t r;
    if (s_horizontal) {
        r.x1 = base_x + lo - 1; r.x2 = base_x + hi - 1;
        r.y1 = by;              r.y2 = by + m->cross - 1;
    } else {
        r.x1 = bx;              r.x2 = bx + m->cross - 1;
        r.y1 = base_y - hi + 1; r.y2 = base_y - lo + 1;
    }
    lv_draw_rect(layer, &dsc, &r);
}

// Custom draw: the filled bar (split into colour zones) plus an optional peak
// marker, painted over the container. Vertical bars grow from the bottom edge
// up; horizontal from the left edge right.
static void bar_draw_cb(lv_event_t *e)
{
    bar_meter_t *m = lv_event_get_user_data(e);
    lv_layer_t  *layer = lv_event_get_layer(e);

    lv_area_t a;
    lv_obj_get_coords(m->cont, &a);

    // Baseline (distance 0) and cross span both inset by the frame width.
    int bx = a.x1 + m->off;             // left/cross origin
    int by = a.y1 + m->off;             // top (horizontal cross origin)
    int base_x = a.x1 + m->off;         // horizontal fill starts here
    int base_y = a.y2 - m->off;         // vertical fill starts here (baseline)

    int z1 = (int)(BAR_ZONE_MID * (float)m->span + 0.5f);
    int z2 = (int)(BAR_ZONE_HOT * (float)m->span + 0.5f);
    uint32_t c_lo = BAR_COLOR_LO;
    if (!s_zones) {
        // Single zone spanning the whole bar, in the theme's bar colour.
        z1 = z2 = m->span;
        c_lo = theme_get()->vu_bar;
    }

    int fill = m->fill < 0 ? 0 : (m->fill > m->span ? m->span : m->fill);
    if (fill > 0) {
        draw_band(layer, m, bx, by, base_x, base_y,
                  1, fill < z1 ? fill : z1, c_lo);
        if (fill > z1) draw_band(layer, m, bx, by, base_x, base_y,
                                 z1 + 1, fill < z2 ? fill : z2, BAR_COLOR_MID);
        if (fill > z2) draw_band(layer, m, bx, by, base_x, base_y,
                                 z2 + 1, fill, BAR_COLOR_HOT);
    }

    if (s_peak && m->peak > 0) {
        int p_hi = m->peak;                 // outer edge (distance from baseline)
        int p_lo = p_hi - BAR_PEAK_W + 1;   // marker band [p_lo .. p_hi]
        if (p_lo < 1) p_lo = 1;
        uint32_t pc = p_hi > z2 ? BAR_COLOR_HOT
                    : p_hi > z1 ? BAR_COLOR_MID : c_lo;
        draw_band(layer, m, bx, by, base_x, base_y, p_lo, p_hi, pc);
    }
}

// Map the smoothed levels to pixel lengths and invalidate only the band of the
// bar that changed (bar edge motion + old/new peak marker) — the rest of the
// container never repaints, so a large bar over a wallpaper stays cheap.
static void meter_render(bar_meter_t *m)
{
    int fill = (int)(m->lvl * (float)m->span + 0.5f);
    if (fill < 0) fill = 0; else if (fill > m->span) fill = m->span;
    int peak = s_peak ? (int)(m->peak_lvl * (float)m->span + 0.5f) : 0;
    if (peak < 0) peak = 0; else if (peak > m->span) peak = m->span;

    if (fill == m->fill && peak == m->peak) return;

    // Changed range along the fill axis (distance from baseline).
    int d_lo = fill < m->fill ? fill : m->fill;
    int d_hi = fill > m->fill ? fill : m->fill;
    if (s_peak) {
        int po = m->peak, pn = peak;
        int pmin = (po < pn ? po : pn) - BAR_PEAK_W;   // cover both marker bands
        int pmax =  po > pn ? po : pn;
        if (pmin < d_lo) d_lo = pmin;
        if (pmax > d_hi) d_hi = pmax;
    }
    if (d_lo < 0) d_lo = 0;
    if (d_hi > m->span) d_hi = m->span;

    m->fill = fill;
    m->peak = peak;

    lv_area_t a;
    lv_obj_get_coords(m->cont, &a);
    int base_x = a.x1 + m->off, base_y = a.y2 - m->off;
    int bx = a.x1 + m->off, by = a.y1 + m->off;
    lv_area_t inv;
    if (s_horizontal) {
        inv.x1 = base_x + d_lo - 1; inv.x2 = base_x + d_hi;
        inv.y1 = by;                inv.y2 = by + m->cross - 1;
    } else {
        inv.x1 = bx;                inv.x2 = bx + m->cross - 1;
        inv.y1 = base_y - d_hi;     inv.y2 = base_y - d_lo + 1;
    }
    lv_obj_invalidate_area(m->cont, &inv);
}

static void smooth_to(bar_meter_t *m, float target)
{
    float k = (target > m->lvl) ? BAR_ATTACK : BAR_DECAY;
    m->lvl += (target - m->lvl) * k;
    // Peak jumps up to the bar instantly, then sags back slowly.
    if (m->lvl > m->peak_lvl) m->peak_lvl = m->lvl;
    else                      m->peak_lvl -= BAR_PEAK_FALL;
    if (m->peak_lvl < 0.0f) m->peak_lvl = 0.0f;
}

// LVGL-task timer: RMS → dB → AGC window → ballistics → bar length. Same chain
// as the needle VU (see vu_needle_widget.c for the reasoning behind the knobs).
static void tick_cb(lv_timer_t *t)
{
    (void)t;
    float target[2] = { 0.0f, 0.0f };

    // No new audio since last tick (paused / stopped) or another source holds
    // the audio path: bars fall to rest. s_last_count stays stale in the
    // foreign-source case so the meter resumes when the owner takes the path back.
    uint32_t count = audio_levels_count();
    if (count != s_last_count && media_source_current() == s_owner) {
        s_last_count = count;

        float rms_l, rms_r;
        audio_levels_get_stereo(&rms_l, &rms_r);
        float db[2] = {
            20.0f * log10f(rms_l + 1e-6f),
            20.0f * log10f(rms_r + 1e-6f),
        };

        float frame_max = db[0] > db[1] ? db[0] : db[1];
        if (frame_max > s_agc_ref) s_agc_ref += (frame_max - s_agc_ref) * BAR_AGC_ATTACK;
        else                       s_agc_ref -= BAR_AGC_FALL_DB;
        if (s_agc_ref < BAR_AGC_REF_MIN) s_agc_ref = BAR_AGC_REF_MIN;

        // Volume is deliberately NOT applied (unlike the needle): the AGC already
        // normalizes to the programme peak, so the bars use the full scale on loud
        // passages regardless of the listening level — classic bar-VU behaviour.
        for (int i = 0; i < 2; i++) {
            float lvl = (db[i] - (s_agc_ref - BAR_AGC_RANGE_DB)) / BAR_AGC_RANGE_DB;
            if (lvl < 0.0f) lvl = 0.0f; else if (lvl > 1.0f) lvl = 1.0f;
            target[i] = powf(lvl, BAR_GAMMA);
        }
    }

    for (int i = 0; i < 2; i++) {
        if (!s_m[i].cont) continue;
        smooth_to(&s_m[i], target[i]);
        meter_render(&s_m[i]);
    }
}

static void meter_create(bar_meter_t *m, lv_obj_t *parent,
                         int16_t x, int16_t y, int16_t w, int16_t h, bool frame)
{
    const ui_theme_colors_t *th = theme_get();

    // Guard degenerate geometry (fresh profile fields can be 0).
    if (w < 8) w = 8;
    if (h < 8) h = 8;

    m->off   = frame ? 1 : 0;   // keep the bar inside the frame instead of over it
    m->span  = (s_horizontal ? w : h) - 2 * m->off;
    m->cross = (s_horizontal ? h : w) - 2 * m->off;
    if (m->span  < 1) m->span  = 1;
    if (m->cross < 1) m->cross = 1;
    m->lvl = 0.0f;
    m->peak_lvl = 0.0f;
    m->fill = 0;
    m->peak = 0;

    m->cont = lv_obj_create(parent);
    lv_obj_remove_style_all(m->cont);
    lv_obj_set_size(m->cont, w, h);
    lv_obj_set_pos(m->cont, x, y);
    lv_obj_clear_flag(m->cont, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    // Opaque bg is the cheap path (changed strips are a solid fill). Transparent
    // shows the screen bg (wallpaper/gradient) under the bar.
    if (!s_transparent) {
        lv_obj_set_style_bg_color(m->cont, lv_color_hex(th->vu_bg), 0);
        lv_obj_set_style_bg_opa(m->cont, LV_OPA_COVER, 0);
    }
    if (frame) {
        lv_obj_set_style_border_color(m->cont, lv_color_hex(th->vu_bg), 0);
        lv_obj_set_style_border_width(m->cont, 1, 0);
        lv_obj_set_style_border_opa(m->cont, LV_OPA_COVER, 0);
    }
    lv_obj_add_event_cb(m->cont, bar_draw_cb, LV_EVENT_DRAW_POST, m);
}

void vu_stereo_widget_create(lv_obj_t *parent,
                             bool show_l, int16_t l_x, int16_t l_y, int16_t l_w, int16_t l_h,
                             bool show_r, int16_t r_x, int16_t r_y, int16_t r_w, int16_t r_h,
                             bool horizontal, bool frame, bool transparent,
                             bool peak, bool zones, media_source_t owner)
{
    if (s_created) return;
    if (!show_l && !show_r) return;
    s_created     = true;
    s_owner       = owner;
    s_horizontal  = horizontal;
    s_transparent = transparent;
    s_peak        = peak;
    s_zones       = zones;

    s_m[0] = (bar_meter_t){ 0 };
    s_m[1] = (bar_meter_t){ 0 };
    if (show_l) meter_create(&s_m[0], parent, l_x, l_y, l_w, l_h, frame);
    if (show_r) meter_create(&s_m[1], parent, r_x, r_y, r_w, r_h, frame);

    s_agc_ref = BAR_AGC_REF_MIN;   // re-anchor to the new programme on recreate
    s_last_count = audio_levels_count();
    s_timer = lv_timer_create(tick_cb, BAR_TICK_MS, NULL);
    ESP_LOGI(TAG, "Created (L:%d R:%d %s peak:%d)", show_l, show_r,
             horizontal ? "horiz" : "vert", peak);
}

void vu_stereo_widget_destroy(void)
{
    if (!s_created) return;
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    for (int i = 0; i < 2; i++) {
        if (s_m[i].cont) { lv_obj_delete(s_m[i].cont); s_m[i].cont = NULL; }
    }
    s_created = false;
    ESP_LOGI(TAG, "Destroyed");
}

void vu_stereo_widget_apply_theme(void)
{
    if (!s_created) return;
    const ui_theme_colors_t *th = theme_get();
    for (int i = 0; i < 2; i++) {
        if (!s_m[i].cont) continue;
        if (!s_transparent) lv_obj_set_style_bg_color(s_m[i].cont, lv_color_hex(th->vu_bg), 0);
        lv_obj_set_style_border_color(s_m[i].cont, lv_color_hex(th->vu_bg), 0);
        lv_obj_invalidate(s_m[i].cont);   // repaint bg/frame (+ bar when zones off)
    }
}
