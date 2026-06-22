#include "ui_anim_bg.h"
#include "ui_profile.h"        // DISPLAY_WIDTH / DISPLAY_HEIGHT
#include "theme.h"             // theme_get()->bg_primary

#if !defined(UI_PROFILE_MONO_128X64) && !defined(UI_PROFILE_MONO_256X64)

#include <stdlib.h>
#include <math.h>

// ── Tunables ────────────────────────────────────────────────────────────────
#define VU_PITCH_PX     12          // target horizontal slot per bar
#define VU_BARS_MAX     64          // static ceiling (widest panel / pitch)
// Rendering on this panel is byte-swap + full-strip invalidation per moving bar,
// all on CPU1 alongside the audio decoders — so the frame rate is the real cost
// knob. Kept low: a background VU reads fine at a handful of fps and leaves CPU1
// for the stream.
#define VU_TICK_MS      160         // animation period (~6 fps)
#define VU_EASE         0.30f       // fraction of the gap closed per tick

// Horizontal colour gradient sampled per bar (left → right).
#define VU_COL_L_R  0xFF  // orange  #FF7A18
#define VU_COL_L_G  0x7A
#define VU_COL_L_B  0x18
#define VU_COL_R_R  0xE6  // magenta #E61E8C
#define VU_COL_R_G  0x1E
#define VU_COL_R_B  0x8C

static lv_obj_t   *s_backdrop = NULL;
static lv_obj_t   *s_bars[VU_BARS_MAX];
static float       s_cur[VU_BARS_MAX];     // current height (px)
static float       s_tgt[VU_BARS_MAX];     // target  height (px)
static lv_timer_t *s_timer = NULL;
static int         s_n       = 0;          // active bar count
static int         s_baseline;             // y of bar bottoms
static int         s_min_h, s_max_h;       // height clamps (px)

static inline float frand(void) { return (float)rand() / (float)RAND_MAX; }

static inline int new_target(void)
{
    return s_min_h + (int)(frand() * (float)(s_max_h - s_min_h));
}

static void tick_cb(lv_timer_t *t)
{
    (void)t;
    for (int i = 0; i < s_n; i++) {
        // Smoothly approach the target; on arrival pick a fresh peak so each bar
        // rises and falls at its own cadence — a VU-meter feel without audio.
        s_cur[i] += (s_tgt[i] - s_cur[i]) * VU_EASE;
        if (fabsf(s_tgt[i] - s_cur[i]) < 1.5f)
            s_tgt[i] = (float)new_target();

        int h = (int)(s_cur[i] + 0.5f);
        if (h < s_min_h) h = s_min_h;
        if (h == lv_obj_get_height(s_bars[i])) continue;   // skip redraw if unchanged

        lv_obj_set_height(s_bars[i], h);
        lv_obj_set_y(s_bars[i], s_baseline - h);           // keep bottoms on the baseline
    }
}

void ui_anim_bg_start(void)
{
    if (s_backdrop) return;   // already running

    const int W = DISPLAY_WIDTH, H = DISPLAY_HEIGHT;

    // Full-screen opaque backdrop on the bottom layer (below the transparent
    // active screen). Bars are its children; deleting it drops the whole tree.
    s_backdrop = lv_obj_create(lv_layer_bottom());
    lv_obj_remove_style_all(s_backdrop);
    lv_obj_set_size(s_backdrop, W, H);
    lv_obj_set_pos(s_backdrop, 0, 0);
    lv_obj_set_style_bg_color(s_backdrop, lv_color_hex(theme_get()->bg_primary), 0);
    lv_obj_set_style_bg_opa(s_backdrop, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_backdrop, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    int n = W / VU_PITCH_PX;
    if (n > VU_BARS_MAX) n = VU_BARS_MAX;
    if (n < 1) n = 1;
    s_n = n;

    const int bar_w  = (VU_PITCH_PX * 6) / 10;             // ~60 % fill, rest is gap
    const int total  = n * VU_PITCH_PX - (VU_PITCH_PX - bar_w);
    const int start_x = (W - total) / 2;

    s_baseline = H - H * 8 / 100;                          // bottom margin ~8 %
    s_max_h    = H * 55 / 100;
    s_min_h    = H * 4 / 100;
    if (s_min_h < 2) s_min_h = 2;

    for (int i = 0; i < n; i++) {
        // Per-bar colour: linear lerp across the orange→magenta gradient.
        float f = (n > 1) ? (float)i / (float)(n - 1) : 0.0f;
        uint8_t r = (uint8_t)(VU_COL_L_R + (VU_COL_R_R - VU_COL_L_R) * f);
        uint8_t g = (uint8_t)(VU_COL_L_G + (VU_COL_R_G - VU_COL_L_G) * f);
        uint8_t b = (uint8_t)(VU_COL_L_B + (VU_COL_R_B - VU_COL_L_B) * f);

        lv_obj_t *bar = lv_obj_create(s_backdrop);
        lv_obj_remove_style_all(bar);
        lv_obj_set_width(bar, bar_w);
        lv_obj_set_style_radius(bar, LV_RADIUS_CIRCLE, 0);  // pill / rounded ends
        lv_obj_set_style_bg_color(bar, lv_color_make(r, g, b), 0);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
        lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

        int h = new_target();
        lv_obj_set_height(bar, h);
        lv_obj_set_pos(bar, start_x + i * VU_PITCH_PX, s_baseline - h);

        s_bars[i] = bar;
        s_cur[i]  = (float)h;
        s_tgt[i]  = (float)new_target();
    }

    s_timer = lv_timer_create(tick_cb, VU_TICK_MS, NULL);
}

void ui_anim_bg_stop(void)
{
    if (s_timer)   { lv_timer_delete(s_timer); s_timer = NULL; }
    if (s_backdrop){ lv_obj_delete(s_backdrop); s_backdrop = NULL; }  // frees the bars too
    s_n = 0;
}

bool ui_anim_bg_active(void) { return s_backdrop != NULL; }

void ui_anim_bg_apply_theme(void)
{
    if (!s_backdrop) return;
    lv_obj_set_style_bg_color(s_backdrop, lv_color_hex(theme_get()->bg_primary), 0);
    lv_obj_invalidate(s_backdrop);
}

#else  // mono panel — no animated background

void ui_anim_bg_start(void) { }
void ui_anim_bg_stop(void)  { }
bool ui_anim_bg_active(void) { return false; }
void ui_anim_bg_apply_theme(void) { }

#endif
