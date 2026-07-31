#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Distance-only swipe detector, for widgets that own the drag themselves.
//
// ui_manager dispatches LVGL's native LV_EVENT_GESTURE as UI_INPUT_SWIPE_*, but
// that gesture needs both distance and speed, and it only fires where the widget
// under the finger lets it bubble. Two places fail that test: list rows (a
// sideways drag stays a plain press, so a swipe that missed the speed floor
// arrives as a tap on the row) and the equalizer's band sliders (gesture
// bubbling is switched off there, or the drag would freeze mid-adjust). Both
// need the same thing — a horizontal swipe recognised by distance alone.
//
// Usage: call ui_swipe_begin() from the widget's LV_EVENT_PRESSED handler and
// ui_swipe_check() from LV_EVENT_PRESSING. The state is a singleton because a
// pointer has one press in flight at a time.

// Start watching a press. Records where the finger landed.
void ui_swipe_begin(void);

// Has the press travelled far enough sideways to be a swipe? If so, sends the
// matching UI_INPUT_SWIPE_LEFT/RIGHT, tells LVGL to swallow the rest of the
// press (so the widget under the finger never sees a click), and returns true —
// the caller's cue to undo whatever the drag did so far. Returns false
// otherwise, including for every call after the swipe already fired.
bool ui_swipe_check(void);

// Whether the current press already fired a swipe — for a click handler that runs
// anyway, because LVGL had already queued the click when the swipe was detected.
bool ui_swipe_fired(void);

#ifdef __cplusplus
}
#endif
