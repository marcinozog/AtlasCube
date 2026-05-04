idf_component_register(
    SRCS
        "ui_manager.c"
        "ui_profile.c"
        "screens/screen_splash.c"
        "screens/img_atlas_logo.c"
        "screens/screen_radio.c"
        "screens/screen_playlist.c"
        "screens/screen_clock.c"
        # "screen_weather.c"
        "screens/screen_bt.c"
        "screens/screen_wifi_ap.c"
        "screens/screen_settings.c"
        "screens/screen_equalizer.c"
        "screens/screen_events.c"
        "screens/screen_event_notification.c"
        "widgets/clock_widget.c"
        "widgets/now_playing_widget.c"
        "widgets/mode_indicator_widget.c"
        "widgets/event_indicator_widget.c"
        "fonts/lv_font_montserrat_12_pl.c"
        "fonts/lv_font_montserrat_14_pl.c"
        "fonts/lv_font_montserrat_18_pl.c"
        "fonts/lv_font_montserrat_72.c"
        "fonts/lv_font_montserrat_80.c"
        "fonts/lv_font_montserrat_96.c"
        "fonts/ui_fonts.c"
    INCLUDE_DIRS
        "."
        "screens"
        "widgets"
    REQUIRES
        lvgl
        audio
        driver
        app_state
        services                # radio_service.h (radio_state_t)
        settings_ex             # settings_set_volume itp. (z screen_radio)
        network                 # screen_wifi
        events                  # event_indicator_widget, screen_event_notification
        esp_wifi                # RSSI w settings mem bar
        json                    # ui_profile JSON I/O
)