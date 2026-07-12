#pragma once

#include "esp_err.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// --------------------------------------------------------------------------
// Model
// --------------------------------------------------------------------------

#define EVENTS_MAX          200
#define EVENT_ID_LEN        9       // 8 hex + '\0'
#define EVENT_TITLE_LEN     64
#define EVENT_SOUND_LEN     128     // SD path (rel. to card root) or /voice WAV, '\0' incl.

// EV_SCHEDULE `station` sentinel: with an empty `sound`, stop whatever is
// playing at the scheduled time instead of starting a source.
#define EVENT_STATION_STOP  (-1)

typedef enum {
    EV_BIRTHDAY = 0,
    EV_NAMEDAY,
    EV_REMINDER,
    EV_ANNIVERSARY,
    EV_VOICE,
    EV_SCHEDULE,
    // Read-only mirror of an external source (phone calendar). Source of truth
    // lives outside the firmware, so these are never fired (no melody/toast) and
    // are excluded from the 🔔 pending counter — they only feed the on-screen
    // calendar widget. Managed in bulk via events_replace_calendar().
    EV_CALENDAR,
    EV_TYPE_COUNT
} event_type_t;

typedef enum {
    EV_REC_NONE = 0,
    EV_REC_DAILY,
    EV_REC_WEEKLY,
    EV_REC_MONTHLY,
    EV_REC_YEARLY,
} event_recurrence_t;

typedef struct {
    char               id[EVENT_ID_LEN];
    event_type_t       type;
    char               title[EVENT_TITLE_LEN];

    // Base date (birthday: year of birth; reminder: year of first occurrence).
    int                year;        // 1970..2100
    int                month;       // 1..12
    int                day;         // 1..31

    int                hour;        // 0..23
    int                minute;      // 0..59

    event_recurrence_t recurrence;
    bool               enabled;

    // EV_SCHEDULE with an empty `sound`: 0-based index into the playlist
    // (resolved at fire time via playlist_get()), or EVENT_STATION_STOP to
    // stop playback instead of starting it.
    int                station;

    // EV_VOICE / EV_SCHEDULE: 0..100, applied via settings_set_volume() at fire
    // time so playback starts at a predictable level regardless of the user's
    // last volume setting.
    int                volume;

    // EV_VOICE: filename of a WAV in /voice on the SD card (generated on the
    // phone via TTS). EV_SCHEDULE: path of an SD audio file or folder relative
    // to the card root (e.g. "/music/wake.mp3" or "/music/morning"); empty
    // means the EV_SCHEDULE plays the playlist `station` instead. Empty for
    // every other type.
    char               sound[EVENT_SOUND_LEN];
} event_t;

// --------------------------------------------------------------------------
// "Event fired" callback — registration from the UI side
// --------------------------------------------------------------------------

typedef void (*events_fire_cb_t)(const event_t *e);

/**
 * Registers a listener called when an event fires
 * (scheduler task — don't do heavy LVGL work here, just push to a queue).
 * Pass NULL to detach.
 */
void events_service_set_fire_cb(events_fire_cb_t cb);

/** Static type label ready for display ("BIRTHDAY", "REMINDER", etc.). */
const char *events_type_label(event_type_t t);

// --------------------------------------------------------------------------
// Lifecycle
// --------------------------------------------------------------------------

/**
 * Loads events.json from SPIFFS, starts the scheduler (esp_timer 30s).
 * Safe if NTP isn't synced yet — the scheduler waits.
 */
esp_err_t events_service_init(void);

/**
 * Hook to call after a timezone change (ntp_service_reconfigure).
 * Resets fired/day_active flags so "today" is recomputed in the new TZ.
 */
void events_service_on_tz_changed(void);

// --------------------------------------------------------------------------
// CRUD (in-memory + autosave do SPIFFS)
// --------------------------------------------------------------------------

/**
 * Appends a new event. If ev->id[0] == '\0', generates a random 8-char id.
 * Writes the generated id back into ev->id.
 */
esp_err_t events_add(event_t *ev);

/** Updates the event with the given id. ev->id may be empty — `id` is used. */
esp_err_t events_update(const char *id, const event_t *ev);

/** Removes the event with the given id. */
esp_err_t events_remove(const char *id);

/** Copies the list to a buffer (for HTTP GET). Returns the number written. */
int events_get_all(event_t *out, int max);

/** Number of active events in memory. */
int events_count(void);

/**
 * Bulk "wipe & replace" of all EV_CALENDAR events: drops every existing
 * calendar event, then appends `arr[0..n)` (their `type` is forced to
 * EV_CALENDAR, a random id is generated when empty). Non-calendar events are
 * left untouched. One SPIFFS write. Excess entries beyond EVENTS_MAX are
 * dropped silently. Called from the bulk sync endpoint.
 */
esp_err_t events_replace_calendar(const event_t *arr, int n);

/**
 * Copies the next upcoming EV_CALENDAR event for today (soonest target time
 * that hasn't passed yet) into `out`. Returns false when NTP isn't synced or
 * nothing is upcoming. Used by the calendar widget (LVGL task) — copies rather
 * than returning a pointer so a concurrent bulk replace can't pull data out.
 */
bool events_calendar_current(event_t *out);

/** Looks up by id; returns an internal pointer or NULL. Do not modify. */
const event_t *events_find(const char *id);

// --------------------------------------------------------------------------
// Stan dzisiejszy (dla indicator widgetu)
// --------------------------------------------------------------------------

/**
 * Number of events scheduled for today that haven't fired yet
 * (target_minute > now_minute). 0 when nothing is upcoming.
 */
int events_pending_today_count(void);

#ifdef __cplusplus
}
#endif
