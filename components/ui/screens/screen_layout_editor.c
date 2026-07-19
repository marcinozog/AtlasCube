#include "screen_layout_editor.h"

#include "fonts/ui_fonts.h"
#include "theme.h"
#include "ui_manager.h"
#include "ui_profile.h"
#include "lvgl.h"
#include "esp_log.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "SCR_LAYOUT";

#define OFF_NONE ((size_t)-1)
#define OFF(field) offsetof(ui_profile_t, field)
#define HOT_OFF(index, field) \
    (offsetof(ui_profile_t, radio_touch_hotspots) + \
     (index) * sizeof(ui_touch_hotspot_t) + offsetof(ui_touch_hotspot_t, field))
#define ARRAY_LEN(a) ((int)(sizeof(a) / sizeof((a)[0])))
#define MAX_EDITOR_ELEMENTS 24

typedef enum {
    SHAPE_NOW_PLAYING,
    SHAPE_STATION_ICON,
    SHAPE_STATE,
    SHAPE_AUDIO_INFO,
    SHAPE_INDICATOR,
    SHAPE_CLOCK,
    SHAPE_VU,
    SHAPE_WHEEL,
    SHAPE_WEATHER,
    SHAPE_HOTSPOT,
} editor_shape_t;

typedef struct {
    const char     *name;
    const char     *sample;
    editor_shape_t  shape;
    size_t          enabled_off;
    size_t          x_off;
    size_t          y_off;
    size_t          w_off;
    size_t          h_off;
    size_t          font_off;
    bool            x_is_center;
    bool            needs_wheels_master;
} editor_element_t;

// This table is the Radio-specific part of the editor. Home and SD can add
// equivalent descriptor tables without duplicating the drag/save UI.
static const editor_element_t RADIO_ELEMENTS[] = {
    { "Now playing", "Atlas Radio", SHAPE_NOW_PLAYING,
      OFF(radio_show_np), OFF(radio_np_x), OFF(radio_np_y), OFF_NONE, OFF_NONE,
      OFF(radio_np_station_font), false, false },
    { "Station icon", "ICON", SHAPE_STATION_ICON,
      OFF(radio_show_station_icon), OFF(radio_station_icon_x), OFF(radio_station_icon_y),
      OFF(radio_station_icon_size), OFF(radio_station_icon_size), OFF_NONE, false, false },
    { "Playback", "PLAYING", SHAPE_STATE,
      OFF(radio_show_playback_status), OFF_NONE, OFF(radio_state_y), OFF_NONE, OFF_NONE,
      OFF(radio_state_font), true, false },
    { "Audio info", "44100 Hz  2ch  128kbps", SHAPE_AUDIO_INFO,
      OFF(radio_show_playback_status), OFF_NONE, OFF(radio_audio_info_y), OFF_NONE, OFF_NONE,
      OFF(radio_audio_info_font), true, false },
    { "Mode", "M", SHAPE_INDICATOR,
      OFF(radio_show_mode_indicator), OFF(radio_mode_indic_x), OFF(radio_mode_indic_y),
      OFF_NONE, OFF_NONE, OFF_NONE, false, false },
    { "Clock", "12:34", SHAPE_CLOCK,
      OFF(radio_show_clock), OFF(radio_clock_widget_x), OFF(radio_clock_widget_y),
      OFF_NONE, OFF_NONE, OFF(radio_clock_font), true, false },
    { "Event", "E", SHAPE_INDICATOR,
      OFF(radio_show_event_indicator), OFF(radio_event_indic_x), OFF(radio_event_indic_y),
      OFF_NONE, OFF_NONE, OFF_NONE, false, false },
    { "VU", "VU", SHAPE_VU,
      OFF(radio_show_vu), OFF(radio_vu_x), OFF(radio_vu_y),
      OFF(radio_vu_w), OFF(radio_vu_h), OFF_NONE, false, false },
    { "Needle L", "L", SHAPE_VU,
      OFF(radio_needle_show_l), OFF(radio_needle_l_x), OFF(radio_needle_l_y),
      OFF(radio_needle_l_w), OFF(radio_needle_l_h), OFF_NONE, false, false },
    { "Needle R", "R", SHAPE_VU,
      OFF(radio_needle_show_r), OFF(radio_needle_r_x), OFF(radio_needle_r_y),
      OFF(radio_needle_r_w), OFF(radio_needle_r_h), OFF_NONE, false, false },
    { "Wheel L", "L", SHAPE_WHEEL,
      OFF(radio_show_wheel_left), OFF(radio_cassette_l_x), OFF(radio_cassette_l_y),
      OFF(radio_cassette_l_size), OFF(radio_cassette_l_size), OFF_NONE, false, true },
    { "Wheel R", "R", SHAPE_WHEEL,
      OFF(radio_show_wheel_right), OFF(radio_cassette_r_x), OFF(radio_cassette_r_y),
      OFF(radio_cassette_r_size), OFF(radio_cassette_r_size), OFF_NONE, false, true },
    { "Weather", "WEATHER", SHAPE_WEATHER,
      OFF(radio_show_weather), OFF(radio_weather_x), OFF(radio_weather_y),
      OFF(radio_weather_w), OFF_NONE, OFF(radio_weather_font), false, false },
    { "Touch 1", "", SHAPE_HOTSPOT,
      HOT_OFF(0, enabled), HOT_OFF(0, x), HOT_OFF(0, y), HOT_OFF(0, w), HOT_OFF(0, h),
      OFF_NONE, false, false },
    { "Touch 2", "", SHAPE_HOTSPOT,
      HOT_OFF(1, enabled), HOT_OFF(1, x), HOT_OFF(1, y), HOT_OFF(1, w), HOT_OFF(1, h),
      OFF_NONE, false, false },
    { "Touch 3", "", SHAPE_HOTSPOT,
      HOT_OFF(2, enabled), HOT_OFF(2, x), HOT_OFF(2, y), HOT_OFF(2, w), HOT_OFF(2, h),
      OFF_NONE, false, false },
    { "Touch 4", "", SHAPE_HOTSPOT,
      HOT_OFF(3, enabled), HOT_OFF(3, x), HOT_OFF(3, y), HOT_OFF(3, w), HOT_OFF(3, h),
      OFF_NONE, false, false },
    { "Touch 5", "", SHAPE_HOTSPOT,
      HOT_OFF(4, enabled), HOT_OFF(4, x), HOT_OFF(4, y), HOT_OFF(4, w), HOT_OFF(4, h),
      OFF_NONE, false, false },
    { "Touch 6", "", SHAPE_HOTSPOT,
      HOT_OFF(5, enabled), HOT_OFF(5, x), HOT_OFF(5, y), HOT_OFF(5, w), HOT_OFF(5, h),
      OFF_NONE, false, false },
};

_Static_assert(ARRAY_LEN(RADIO_ELEMENTS) <= MAX_EDITOR_ELEMENTS,
               "Increase MAX_EDITOR_ELEMENTS");

static layout_editor_target_t s_target = LAYOUT_EDITOR_RADIO;
static ui_profile_t           s_original;
static ui_profile_t           s_work;
static lv_obj_t              *s_root;
static lv_obj_t              *s_boxes[MAX_EDITOR_ELEMENTS];
static lv_obj_t              *s_status;
static lv_obj_t              *s_hint;
static lv_obj_t              *s_visibility_btn;
static lv_obj_t              *s_visibility_label;
static lv_obj_t              *s_exit_overlay;
static lv_obj_t              *s_exit_title;
static int                    s_selected = -1;
static bool                   s_axis_y;
static lv_point_t             s_press_point;
static int                    s_press_left;
static int                    s_press_top;

static int16_t *i16_field(size_t off)
{
    return off == OFF_NONE ? NULL : (int16_t *)((uint8_t *)&s_work + off);
}

static bool bool_field(size_t off)
{
    return off != OFF_NONE && *(bool *)((uint8_t *)&s_work + off);
}

static bool *bool_field_ptr(size_t off)
{
    return off == OFF_NONE ? NULL : (bool *)((uint8_t *)&s_work + off);
}

static const lv_font_t *font_field(size_t off)
{
    if (off == OFF_NONE) return &lv_font_montserrat_10_pl;
    const lv_font_t *font = *(const lv_font_t **)((uint8_t *)&s_work + off);
    return font ? font : &lv_font_montserrat_10_pl;
}

static ui_touch_hotspot_t *hotspot_for(const editor_element_t *d)
{
    if (d->shape != SHAPE_HOTSPOT) return NULL;
    size_t base = d->x_off - offsetof(ui_touch_hotspot_t, x);
    return (ui_touch_hotspot_t *)((uint8_t *)&s_work + base);
}

static const char *hotspot_action_name(int action)
{
    static const char *NAMES[] = {
        "PLAY", "PREV", "NEXT", "VOL-", "VOL+", "STOP", "PAUSE",
        "PLAYLIST", "SD BROWSER"
    };
    return action >= 0 && action < ARRAY_LEN(NAMES) ? NAMES[action] : "?";
}

static const editor_element_t *elements(void) { return RADIO_ELEMENTS; }
static int element_count(void) { return ARRAY_LEN(RADIO_ELEMENTS); }

static bool element_enabled(const editor_element_t *d)
{
    if (!bool_field(d->enabled_off)) return false;
    if (d->needs_wheels_master && !s_work.radio_show_cassette) return false;
    return true;
}

static int clamp_int(int value, int lo, int hi)
{
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

static void element_geometry(const editor_element_t *d,
                             int *left, int *top, int *w, int *h)
{
    int x = d->x_off == OFF_NONE ? DISPLAY_WIDTH / 2 : *i16_field(d->x_off);
    int y = *i16_field(d->y_off);
    int width = d->w_off == OFF_NONE ? 0 : *i16_field(d->w_off);
    int height = d->h_off == OFF_NONE ? 0 : *i16_field(d->h_off);
    const lv_font_t *font = font_field(d->font_off);
    int line_h = lv_font_get_line_height(font);

    switch (d->shape) {
        case SHAPE_NOW_PLAYING:
            width = DISPLAY_WIDTH - x - 10;
            if (width < 8) width = 8;
            height = line_h;
            if (s_work.radio_show_np_title) {
                height += lv_font_get_line_height(font_field(OFF(radio_np_title_font))) + 4;
            }
            break;
        case SHAPE_STATION_ICON:
        case SHAPE_WHEEL:
            height = width;
            break;
        case SHAPE_STATE:
            width = DISPLAY_WIDTH < 120 ? DISPLAY_WIDTH - 8 : 112;
            height = line_h;
            break;
        case SHAPE_AUDIO_INFO:
            width = DISPLAY_WIDTH - 8;
            if (width > 236) width = 236;
            height = line_h;
            break;
        case SHAPE_INDICATOR:
            width = height = 16;
            break;
        case SHAPE_CLOCK:
            width = line_h * 3;
            if (width < 38) width = 38;
            height = line_h;
            break;
        case SHAPE_VU:
            if (width < 8) width = 8;
            if (height < 8) height = 8;
            break;
        case SHAPE_WEATHER:
            if (width <= 0) width = DISPLAY_WIDTH;
            height = line_h < 20 ? 20 : line_h;
            break;
        case SHAPE_HOTSPOT:
            if (width < 8) width = 8;
            if (height < 8) height = 8;
            break;
    }

    if (d->x_off == OFF_NONE || d->x_is_center) x -= width / 2;
    *left = x;
    *top = y;
    *w = width;
    *h = height;
}

static void set_element_position(int index, int left, int top)
{
    const editor_element_t *d = &elements()[index];
    int old_left, old_top, w, h;
    element_geometry(d, &old_left, &old_top, &w, &h);
    (void)old_left;
    (void)old_top;

    // Leave at least an 8 px grab handle on-screen while still allowing the
    // partially off-screen placements used by some layouts.
    left = clamp_int(left, 8 - w, DISPLAY_WIDTH - 8);
    top  = clamp_int(top,  8 - h, DISPLAY_HEIGHT - 8);

    if (d->x_off != OFF_NONE) {
        int x = d->x_is_center ? left + w / 2 : left;
        *i16_field(d->x_off) = (int16_t)x;
    }
    *i16_field(d->y_off) = (int16_t)top;
}

static void style_box(int index)
{
    lv_obj_t *box = s_boxes[index];
    if (!box) return;
    bool selected = index == s_selected;
    bool hotspot = elements()[index].shape == SHAPE_HOTSPOT;
    const ui_theme_colors_t *th = theme_get();
    lv_obj_set_style_border_color(box,
        selected ? lv_color_hex(th->accent)
                 : hotspot ? lv_color_hex(th->status_ok) : lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_border_width(box, selected ? 3 : 1, LV_PART_MAIN);
    lv_obj_set_style_border_opa(box, selected ? LV_OPA_COVER : LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_bg_color(box,
        hotspot ? lv_color_hex(th->status_ok) : lv_color_hex(th->bg_primary), LV_PART_MAIN);
    bool enabled = element_enabled(&elements()[index]);
    // Hidden templates stay selectable. Only the currently selected hidden
    // box is click-through, because it is temporarily raised above the other
    // elements and would otherwise block picking a visible box underneath.
    if (enabled || !selected) lv_obj_add_flag(box, LV_OBJ_FLAG_CLICKABLE);
    else                      lv_obj_remove_flag(box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(box, selected ? LV_OPA_40
                                           : enabled ? LV_OPA_20 : LV_OPA_TRANSP,
                            LV_PART_MAIN);
    lv_obj_set_style_opa(box, selected ? LV_OPA_COVER
                                       : enabled ? LV_OPA_COVER : LV_OPA_30,
                         LV_PART_MAIN);
}

static void restack_boxes(void)
{
    // Hidden templates live below active UI elements, so their larger default
    // hitboxes cannot steal taps. They remain clickable wherever they are not
    // covered by a visible element.
    for (int i = 0; i < element_count(); ++i) {
        if (s_boxes[i] && !element_enabled(&elements()[i]))
            lv_obj_move_to_index(s_boxes[i], 0);
    }
    for (int i = 0; i < element_count(); ++i) {
        if (s_boxes[i] && element_enabled(&elements()[i]))
            lv_obj_move_foreground(s_boxes[i]);
    }
    if (s_selected >= 0 && s_boxes[s_selected])
        lv_obj_move_foreground(s_boxes[s_selected]);
}

static void position_box(int index)
{
    lv_obj_t *box = s_boxes[index];
    if (!box) return;
    int left, top, w, h;
    element_geometry(&elements()[index], &left, &top, &w, &h);
    lv_obj_set_pos(box, left, top);
    lv_obj_set_size(box, w, h);
}

static void update_status(void)
{
    if (!s_status) return;
    if (s_selected < 0) {
        lv_label_set_text(s_status, "Radio layout - tap an element");
        if (s_visibility_btn) lv_obj_add_flag(s_visibility_btn, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    const editor_element_t *d = &elements()[s_selected];
    char buf[80];
    int y = *i16_field(d->y_off);
    const char *visibility = element_enabled(d) ? "" : " [HIDDEN]";
    if (d->x_off == OFF_NONE) {
        snprintf(buf, sizeof(buf), "%s%s y:%d", d->name, visibility, y);
    } else {
        int x = *i16_field(d->x_off);
        snprintf(buf, sizeof(buf), "%s%s x:%d y:%d %c",
                 d->name, visibility, x, y, s_axis_y ? 'Y' : 'X');
    }
    lv_label_set_text(s_status, buf);
    if (s_visibility_btn && s_visibility_label) {
        lv_label_set_text(s_visibility_label, element_enabled(d) ? "HIDE" : "SHOW");
        lv_obj_remove_flag(s_visibility_btn, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_move_foreground(s_status);
    if (s_hint) lv_obj_move_foreground(s_hint);
    if (s_visibility_btn) lv_obj_move_foreground(s_visibility_btn);
}

static void select_element(int index)
{
    int previous = s_selected;
    s_selected = index;
    if (elements()[index].x_off == OFF_NONE) s_axis_y = true;
    else if (previous != index) s_axis_y = false;
    if (previous >= 0) style_box(previous);
    style_box(index);
    restack_boxes();
    update_status();
}

static void select_relative(int delta)
{
    if (element_count() <= 0) return;
    int index = s_selected;
    if (index < 0) index = delta > 0 ? -1 : 0;
    index = (index + delta + element_count()) % element_count();
    select_element(index);
}

static void toggle_selected_visibility(void)
{
    if (s_selected < 0) return;
    const editor_element_t *d = &elements()[s_selected];
    bool *flag = bool_field_ptr(d->enabled_off);
    if (!flag) return;

    bool currently_visible = element_enabled(d);
    if (d->needs_wheels_master) {
        if (currently_visible) {
            *flag = false;
            if (!s_work.radio_show_wheel_left && !s_work.radio_show_wheel_right)
                s_work.radio_show_cassette = false;
        } else {
            if (!s_work.radio_show_cassette) {
                s_work.radio_show_wheel_left = false;
                s_work.radio_show_wheel_right = false;
            }
            s_work.radio_show_cassette = true;
            *flag = true;
        }
    } else {
        *flag = !*flag;
    }

    // Some descriptors share one visibility field (playback + audio info), so
    // refresh every template instead of only the selected one.
    for (int i = 0; i < element_count(); ++i) style_box(i);
    restack_boxes();
    update_status();
}

static void visibility_clicked_cb(lv_event_t *e)
{
    (void)e;
    toggle_selected_visibility();
}

static void status_clicked_cb(lv_event_t *e)
{
    (void)e;
    select_relative(1);
}

static void element_event_cb(lv_event_t *e)
{
    int index = (int)(intptr_t)lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t *indev = lv_indev_active();
    if (!indev || index < 0 || index >= element_count()) return;

    if (code == LV_EVENT_PRESSED) {
        select_element(index);
        lv_indev_get_point(indev, &s_press_point);
        int w, h;
        element_geometry(&elements()[index], &s_press_left, &s_press_top, &w, &h);
        (void)w;
        (void)h;
    } else if (code == LV_EVENT_PRESSING) {
        lv_point_t point;
        lv_indev_get_point(indev, &point);
        set_element_position(index,
            s_press_left + point.x - s_press_point.x,
            s_press_top  + point.y - s_press_point.y);
        position_box(index);
        update_status();
    }
}

static void create_template(int index)
{
    const editor_element_t *d = &elements()[index];

    lv_obj_t *box = lv_obj_create(s_root);
    s_boxes[index] = box;
    lv_obj_remove_style_all(box);
    lv_obj_add_flag(box, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(box, element_event_cb, LV_EVENT_PRESSED,  (void *)(intptr_t)index);
    lv_obj_add_event_cb(box, element_event_cb, LV_EVENT_PRESSING, (void *)(intptr_t)index);

    position_box(index);

    if (d->shape == SHAPE_WHEEL) {
        lv_obj_set_style_radius(box, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    } else if (d->shape == SHAPE_HOTSPOT) {
        ui_touch_hotspot_t *hotspot = hotspot_for(d);
        int radius = hotspot ? clamp_int(hotspot->radius, 0, 100) : 0;
        int shorter = lv_obj_get_width(box) < lv_obj_get_height(box)
                    ? lv_obj_get_width(box) : lv_obj_get_height(box);
        lv_obj_set_style_radius(box, (shorter * radius) / 200, LV_PART_MAIN);
    } else {
        lv_obj_set_style_radius(box, 5, LV_PART_MAIN);
    }

    lv_obj_t *label = lv_label_create(box);
    if (d->shape == SHAPE_HOTSPOT) {
        char text[32];
        ui_touch_hotspot_t *hotspot = hotspot_for(d);
        snprintf(text, sizeof(text), "%s\n%s", d->name,
                 hotspot_action_name(hotspot ? hotspot->action : -1));
        lv_label_set_text(label, text);
    } else {
        lv_label_set_text(label, d->sample);
    }
    lv_obj_set_style_text_font(label, font_field(d->font_off), LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(label, LV_PCT(94));
    lv_obj_center(label);
    lv_obj_remove_flag(label, LV_OBJ_FLAG_CLICKABLE);

    style_box(index);
}

typedef enum {
    EXIT_SAVE,
    EXIT_DISCARD,
    EXIT_CONTINUE,
} exit_action_t;

static void exit_action_cb(lv_event_t *e)
{
    exit_action_t action = (exit_action_t)(intptr_t)lv_event_get_user_data(e);
    if (action == EXIT_CONTINUE) {
        lv_obj_del(s_exit_overlay);
        s_exit_overlay = NULL;
        s_exit_title = NULL;
        return;
    }
    if (action == EXIT_DISCARD) {
        ui_navigate(SCREEN_SETTINGS);
        return;
    }

    ui_profile_set(&s_work);
    esp_err_t err = ui_profile_save_to_file();
    if (err != ESP_OK) {
        // Keep a failed save transactional: runtime must still match the last
        // persisted profile while the editor retains s_work for a retry.
        ui_profile_set(&s_original);
        if (s_exit_title) lv_label_set_text(s_exit_title, "Save failed");
        ESP_LOGE(TAG, "Could not save profile: %s", esp_err_to_name(err));
        return;
    }
    ui_navigate(SCREEN_RADIO);
}

static lv_obj_t *dialog_button(lv_obj_t *parent, int x, int y, int w,
                               const char *text, exit_action_t action)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, 34);
    lv_obj_add_event_cb(btn, exit_action_cb, LV_EVENT_CLICKED,
                        (void *)(intptr_t)action);
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_10_pl, LV_PART_MAIN);
    lv_obj_center(label);
    return btn;
}

static void show_exit_dialog(void)
{
    if (!s_root || s_exit_overlay) return;
    s_exit_overlay = lv_obj_create(s_root);
    lv_obj_remove_style_all(s_exit_overlay);
    lv_obj_set_size(s_exit_overlay, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_set_pos(s_exit_overlay, 0, 0);
    lv_obj_set_style_bg_color(s_exit_overlay, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_exit_overlay, LV_OPA_80, LV_PART_MAIN);
    lv_obj_add_flag(s_exit_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(s_exit_overlay, LV_OBJ_FLAG_GESTURE_BUBBLE);

    s_exit_title = lv_label_create(s_exit_overlay);
    lv_label_set_text(s_exit_title, "Finish Radio layout?");
    lv_obj_set_style_text_font(s_exit_title, &lv_font_montserrat_14_pl, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_exit_title, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(s_exit_title, LV_ALIGN_CENTER, 0, -30);

    int gap = 4;
    int bw = (DISPLAY_WIDTH - 16 - gap * 2) / 3;
    if (bw > 100) bw = 100;
    int total = bw * 3 + gap * 2;
    int x = (DISPLAY_WIDTH - total) / 2;
    int y = DISPLAY_HEIGHT / 2;
    dialog_button(s_exit_overlay, x,                y, bw, "SAVE", EXIT_SAVE);
    dialog_button(s_exit_overlay, x + bw + gap,     y, bw, "DISCARD", EXIT_DISCARD);
    dialog_button(s_exit_overlay, x + 2*(bw + gap), y, bw, "BACK", EXIT_CONTINUE);
    lv_obj_move_foreground(s_exit_overlay);
}

static void layout_create(lv_obj_t *parent)
{
    s_root = parent;
    s_original = *ui_profile_get();
    s_work = s_original;
    memset(s_boxes, 0, sizeof(s_boxes));
    s_selected = -1;
    s_axis_y = false;
    s_exit_overlay = NULL;
    s_exit_title = NULL;
    s_visibility_btn = NULL;
    s_visibility_label = NULL;

    const ui_theme_colors_t *th = theme_get();
    lv_obj_set_style_bg_color(parent, lv_color_hex(th->bg_primary), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, LV_PART_MAIN);

    for (int i = 0; i < element_count(); ++i) create_template(i);
    restack_boxes();

    s_status = lv_label_create(parent);
    lv_obj_set_style_text_font(s_status, &lv_font_montserrat_10_pl, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_status, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_status, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_status, LV_OPA_60, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(s_status, 5, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(s_status, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(s_status, 5, LV_PART_MAIN);
    lv_obj_set_width(s_status, DISPLAY_WIDTH - 54);
    lv_label_set_long_mode(s_status, LV_LABEL_LONG_DOT);
    lv_obj_align(s_status, LV_ALIGN_TOP_LEFT, 2, 2);
    lv_obj_add_flag(s_status, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_status, status_clicked_cb, LV_EVENT_CLICKED, NULL);

    s_visibility_btn = lv_btn_create(parent);
    lv_obj_set_size(s_visibility_btn, 48, 24);
    lv_obj_align(s_visibility_btn, LV_ALIGN_TOP_RIGHT, -2, 2);
    lv_obj_add_event_cb(s_visibility_btn, visibility_clicked_cb, LV_EVENT_CLICKED, NULL);
    s_visibility_label = lv_label_create(s_visibility_btn);
    lv_obj_set_style_text_font(s_visibility_label, &lv_font_montserrat_10_pl, LV_PART_MAIN);
    lv_obj_center(s_visibility_label);
    lv_obj_add_flag(s_visibility_btn, LV_OBJ_FLAG_HIDDEN);

    s_hint = lv_label_create(parent);
    lv_label_set_text(s_hint, "drag | tap title:next | side:finish");
    lv_obj_set_style_text_font(s_hint, &lv_font_montserrat_8_pl, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_hint, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_hint, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_hint, LV_OPA_60, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(s_hint, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(s_hint, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(s_hint, 4, LV_PART_MAIN);
    lv_obj_align(s_hint, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_obj_remove_flag(s_hint, LV_OBJ_FLAG_CLICKABLE);

    update_status();
    ESP_LOGI(TAG, "Created target=%d elements=%d", s_target, element_count());
}

static void layout_destroy(void)
{
    s_root = s_status = s_hint = s_visibility_btn = s_visibility_label = NULL;
    s_exit_overlay = s_exit_title = NULL;
    memset(s_boxes, 0, sizeof(s_boxes));
    s_selected = -1;
    ESP_LOGI(TAG, "Destroyed");
}

static void layout_on_event(const ui_event_t *ev) { (void)ev; }

static void layout_on_input(ui_input_t input)
{
    if (s_exit_overlay) return;
    switch (input) {
        case UI_INPUT_ENCODER_CW:
        case UI_INPUT_ENCODER_CCW: {
            if (s_selected < 0) {
                for (int i = 0; i < element_count(); ++i) {
                    if (s_boxes[i]) { select_element(i); break; }
                }
                return;
            }
            int delta = input == UI_INPUT_ENCODER_CW ? 1 : -1;
            int left, top, w, h;
            element_geometry(&elements()[s_selected], &left, &top, &w, &h);
            (void)w;
            (void)h;
            if (s_axis_y || elements()[s_selected].x_off == OFF_NONE) top += delta;
            else left += delta;
            set_element_position(s_selected, left, top);
            position_box(s_selected);
            update_status();
            break;
        }
        case UI_INPUT_ENCODER_PRESS:
            if (s_selected >= 0 && elements()[s_selected].x_off != OFF_NONE) {
                s_axis_y = !s_axis_y;
                update_status();
            }
            break;
        case UI_INPUT_BTN_OK:
            toggle_selected_visibility();
            break;
        case UI_INPUT_SWIPE_UP:
            select_relative(-1);
            break;
        case UI_INPUT_SWIPE_DOWN:
            select_relative(1);
            break;
        case UI_INPUT_ENCODER_LONG_PRESS:
        case UI_INPUT_SWIPE_LEFT:
        case UI_INPUT_SWIPE_RIGHT:
            show_exit_dialog();
            break;
        default:
            break;
    }
}

static void layout_apply_theme(void)
{
    if (!s_root) return;
    for (int i = 0; i < element_count(); ++i) style_box(i);
    lv_obj_invalidate(s_root);
}

void screen_layout_editor_open(layout_editor_target_t target)
{
    s_target = target;
    ui_navigate(SCREEN_LAYOUT_EDITOR);
}

const ui_screen_t screen_layout_editor = {
    .create      = layout_create,
    .destroy     = layout_destroy,
    .apply_theme = layout_apply_theme,
    .on_event    = layout_on_event,
    .on_input    = layout_on_input,
    .name        = "layout_editor",
};
