#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Small solid pill on lv_layer_top() shown while an internet-wallpaper fetch
// runs — the radio goes silent for those seconds and this says why. Lives on
// the top layer, so it survives screen navigation without per-screen wiring.
// Call only from the LVGL task (ui_manager routes UI_EVT_NET_WP here).

// Show "Updating wallpaper..." (idempotent; also cancels a lingering fail message).
void net_fetch_overlay_show(void);

// Fetch finished: ok hides the pill immediately, a failure swaps the text to
// "Wallpaper update failed" and auto-hides after a few seconds.
void net_fetch_overlay_done(bool ok);

#ifdef __cplusplus
}
#endif
