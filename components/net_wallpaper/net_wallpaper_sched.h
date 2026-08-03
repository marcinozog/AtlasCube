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

// Register the boot-batch completion callback BEFORE net_wallpaper_sched_init().
// The boot batch stops any playing radio for its whole duration, so the caller
// runs it FIRST and starts playback from this callback instead — no music is
// interrupted, and the wallpapers are up before the user looks at the screen.
// Fires exactly once, on the first completion of the boot batch, whether it
// succeeded or not (a failed batch retries 15 minutes later; the radio must not
// wait for that). Runs on the fetch task, the same context that already resumes
// playback at the tail of a manual fetch. Pass NULL to clear.
void net_wallpaper_sched_set_boot_done_cb(void (*cb)(void));

// True when a boot batch is actually coming and the callback above will fire —
// i.e. the fetch mode is on AND at least one slot has a URL. Callers that gate
// playback on the callback MUST check this: with nothing to fetch, nothing
// would ever release the gate. Meaningful only right after init.
bool net_wallpaper_sched_boot_fetch_pending(void);

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
