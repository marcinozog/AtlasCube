#include "ui_asset.h"
#include "lv_bin_image.h"
#include "ui_profile.h"
#include "net_asset.h"
#include <string.h>

// "asset0".."asset<N-1>" → slot index, -1 for anything else (a path, "", …).
// Single digit only, matching the ui_background.c parser for "net0".."net9".
static int slot_of(const char *ref)
{
    if (!ref || strncmp(ref, "asset", 5) != 0) return -1;
    if (ref[5] == '\0' || ref[6] != '\0') return -1;   // "asset", "asset12", …
    const int n = ref[5] - '0';
    return (n >= 0 && n < NET_ASSET_SLOTS) ? n : -1;
}

bool ui_asset_is_slot(const char *ref)
{
    return slot_of(ref) >= 0;
}

lv_image_dsc_t *ui_asset_load(const char *ref)
{
    if (!ref || !ref[0]) return NULL;

    const int slot = slot_of(ref);
    if (slot < 0) return lv_bin_image_load(ref, 0, 0);   // SD path

    const lv_image_dsc_t *shared = net_asset_image(slot);
    if (!shared) return NULL;        // nothing fetched into this slot (yet)
    return lv_bin_image_scale_copy(shared, shared->header.w, shared->header.h);
}

bool ui_asset_screen_uses_slots(ui_screen_id_t screen)
{
    const ui_profile_t *p = ui_profile_get();
    switch (screen) {
        case SCREEN_RADIO: return slot_of(p->radio_volslider_knob_image) >= 0;
        case SCREEN_SD:    return slot_of(p->sd_volslider_knob_image)    >= 0;
        case SCREEN_BT:    return slot_of(p->bt_volslider_knob_image)    >= 0;
        case SCREEN_EQ:    return slot_of(p->eq_knob_image)              >= 0;
        default:           return false;   // no other screen takes an image field
    }
}
