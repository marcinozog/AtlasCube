#pragma once

#include "ui_screen.h"
#include "ui_events.h"

extern const ui_screen_t screen_event_notification;

/** Set the event data to display on next navigation to this screen. */
void screen_event_notification_set_info(const ui_event_info_t *info);

/** Set the screen to return to on dismiss. */
void screen_event_notification_set_return(ui_screen_id_t return_to);
