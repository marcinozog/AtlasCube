#include "ui_swipe.h"
#include "ui_manager.h"
#include "ui_events.h"
#include "lvgl.h"

// How far the finger has to travel sideways before the press counts as a swipe.
// Well below LVGL's own gesture limit (50 px) — that is the whole point — but far
// enough that the wobble of a tap or of a vertical fader drag never reaches it.
#define UI_SWIPE_MIN_PX 30

static lv_point_t s_start;
static bool       s_fired = false;

void ui_swipe_begin(void)
{
    s_fired = false;
    lv_indev_t *indev = lv_indev_active();
    if (indev) lv_indev_get_point(indev, &s_start);
}

bool ui_swipe_check(void)
{
    if (s_fired) return false;

    lv_indev_t *indev = lv_indev_active();
    if (!indev) return false;
    // LVGL already committed this press to a scroll (a list being flicked
    // through): the drag belongs to the scroll, not to us.
    if (lv_indev_get_scroll_obj(indev)) return false;

    lv_point_t p;
    lv_indev_get_point(indev, &p);
    int32_t dx = p.x - s_start.x;
    int32_t dy = p.y - s_start.y;

    // Sideways travel only, and it has to beat the vertical component — vertical
    // drags are the band sliders' and the lists' own business.
    if (LV_ABS(dx) < UI_SWIPE_MIN_PX || LV_ABS(dx) <= LV_ABS(dy)) return false;

    s_fired = true;
    lv_indev_wait_release(indev);
    ui_input_send(dx > 0 ? UI_INPUT_SWIPE_RIGHT : UI_INPUT_SWIPE_LEFT);
    return true;
}

bool ui_swipe_fired(void)
{
    return s_fired;
}
