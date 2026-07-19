#include "touch_hotspots_widget.h"

#include <stdint.h>

static lv_obj_t *s_hotspots[UI_TOUCH_HOTSPOT_COUNT];
static control_source_t s_source;

static void hotspot_clicked_cb(lv_event_t *e)
{
    control_action_t action = (control_action_t)(intptr_t)lv_event_get_user_data(e);
    control_action_execute(s_source, action);
}

void touch_hotspots_widget_create(lv_obj_t *parent, control_source_t source,
                                  const ui_touch_hotspot_t *hotspots)
{
    if (!parent || !hotspots) return;
    touch_hotspots_widget_destroy();
    s_source = source;

    for (int i = 0; i < UI_TOUCH_HOTSPOT_COUNT; ++i) {
        const ui_touch_hotspot_t *cfg = &hotspots[i];
        if (!cfg->enabled || cfg->w <= 0 || cfg->h <= 0 ||
            cfg->action < CONTROL_ACTION_PLAY_TOGGLE ||
            cfg->action > CONTROL_ACTION_OPEN_SD_BROWSER) continue;

        lv_obj_t *btn = lv_btn_create(parent);
        s_hotspots[i] = btn;
        lv_obj_remove_style_all(btn);
        lv_obj_set_pos(btn, cfg->x, cfg->y);
        lv_obj_set_size(btn, cfg->w, cfg->h);
        lv_obj_set_style_radius(btn,
            (LV_MIN(cfg->w, cfg->h) * LV_CLAMP(0, cfg->radius, 100)) / 200,
            LV_PART_MAIN);

        // Invisible at rest. The wallpaper supplies the button artwork; only
        // a short translucent highlight reveals the press.
        lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
        lv_obj_set_style_bg_color(btn, lv_color_white(), LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(btn, LV_OPA_40, LV_STATE_PRESSED);
        lv_obj_set_style_border_color(btn, lv_color_white(), LV_STATE_PRESSED);
        lv_obj_set_style_border_opa(btn, LV_OPA_60, LV_STATE_PRESSED);
        lv_obj_set_style_border_width(btn, 1, LV_STATE_PRESSED);
        lv_obj_add_event_cb(btn, hotspot_clicked_cb, LV_EVENT_SHORT_CLICKED,
                            (void *)(intptr_t)cfg->action);
        if (cfg->action == CONTROL_ACTION_VOLUME_DOWN ||
            cfg->action == CONTROL_ACTION_VOLUME_UP) {
            lv_obj_add_event_cb(btn, hotspot_clicked_cb, LV_EVENT_LONG_PRESSED_REPEAT,
                                (void *)(intptr_t)cfg->action);
        }
    }
}

void touch_hotspots_widget_destroy(void)
{
    for (int i = 0; i < UI_TOUCH_HOTSPOT_COUNT; ++i) {
        if (s_hotspots[i]) {
            lv_obj_del(s_hotspots[i]);
            s_hotspots[i] = NULL;
        }
    }
}
