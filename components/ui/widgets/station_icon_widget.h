#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Loads the current playlist entry's optional SD-card icon and attaches it to
// now_playing_widget. All functions must be called from the LVGL task.
void station_icon_widget_create(void);
void station_icon_widget_update(void);
void station_icon_widget_destroy(void);

#ifdef __cplusplus
}
#endif
