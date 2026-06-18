#include "ui_label.h"

typedef struct {
    int16_t x;
    int16_t y;
    uint8_t align;
} anchor_t;

// Re-run on every size change (label auto-sizes when its text changes): shift
// the top-left so the chosen anchor point lands back on (x, y). Position-only
// change, so it does not re-trigger a size event.
static void on_size_changed(lv_event_t *e)
{
    lv_obj_t   *label = lv_event_get_target(e);
    anchor_t   *a     = lv_event_get_user_data(e);
    lv_coord_t  w     = lv_obj_get_width(label);

    int x = a->x;
    if      (a->align == UI_ALIGN_CENTER) x -= w / 2;
    else if (a->align == UI_ALIGN_RIGHT)  x -= w;

    lv_obj_set_pos(label, x, a->y);
}

static void on_delete(lv_event_t *e)
{
    lv_free(lv_event_get_user_data(e));
}

lv_obj_t *ui_anchored_label(lv_obj_t *parent, int x, int y, ui_label_align_t align)
{
    lv_obj_t *label = lv_label_create(parent);

    anchor_t *a = lv_malloc(sizeof(*a));
    a->x     = x;
    a->y     = y;
    a->align = align;

    lv_obj_add_event_cb(label, on_size_changed, LV_EVENT_SIZE_CHANGED, a);
    lv_obj_add_event_cb(label, on_delete,       LV_EVENT_DELETE,       a);

    lv_obj_set_pos(label, x, y);  // initial; refined on first text/size change
    return label;
}
