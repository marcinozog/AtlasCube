#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Automatic internet-wallpaper refresh, driven by settings
// (display.wallpaper_url + wallpaper_fetch_mode: 0=off, 1=once after boot,
// 2=daily at wallpaper_fetch_hour:min — daily also fetches once after boot,
// since the PSRAM wallpaper doesn't survive a reboot). A single lazily-created
// esp_timer one-shot is armed only while there is a next event to wait for and
// stopped otherwise — no periodic tick, no parked tasks (see disarm() for why
// the idle handle is stopped rather than deleted).

// Call once from the STA boot path (needs internet + NTP for the daily mode).
// panel_w/h feed net_wallpaper_fetch's {w}/{h} substitution.
void net_wallpaper_sched_init(int panel_w, int panel_h);

// Recompute + re-arm after the wallpaper-fetch settings changed (the settings
// setter deliberately doesn't call this — settings_ex must not depend on
// net_wallpaper). No-op before init (e.g. AP mode).
void net_wallpaper_sched_update(void);

// Internal: net_wallpaper's fetch task reports every finished fetch (manual
// ones too) so the scheduler can retry a failed scheduled fetch and re-arm.
void net_wallpaper_sched_fetch_done(bool ok);

#ifdef __cplusplus
}
#endif
