idf_component_register(
    SRCS
        "ui_manager.c"
        "ui_nav.c"
        "ui_profile.c"
        "ui_background.c"
        "lv_bin_image.c"
        "ui_label.c"
        "screens/screen_splash.c"
        "screens/img_atlas_logo.c"
        "screens/screen_radio.c"
        "screens/screen_playlist.c"
        "screens/screen_sd_player.c"
        "screens/screen_sd_browser.c"
        "screens/screen_clock.c"
        # "screen_weather.c"
        "screens/screen_bt.c"
        "screens/screen_wifi_ap.c"
        "screens/screen_settings.c"
        "screens/screen_equalizer.c"
        "screens/screen_events.c"
        "screens/screen_event_notification.c"
        "screens/screen_mqtt.c"
        "screens/screen_ota.c"
        "screensavers/screensaver_starfield.c"
        "screensavers/screensaver_fireworks.c"
        "screensavers/screensaver_plasma.c"
        "screensavers/screensaver_clockhands.c"
        "screensavers/screensaver_clockdigital.c"
        "screensavers/screensaver_life.c"
        "screensavers/screensaver_dashboard.c"
        "screensavers/screensaver_mqtt.c"
        "screensavers/screensaver_photo.c"
        "screensavers/screensaver_blank.c"
        "screensavers/screensavers.c"
        "widgets/clock_widget.c"
        "widgets/now_playing_widget.c"
        "widgets/mode_indicator_widget.c"
        "widgets/event_indicator_widget.c"
        "widgets/controls_overlay_widget.c"
        "widgets/vol_overlay_widget.c"
        "fonts/lv_font_montserrat_8_pl.c"
        "fonts/lv_font_montserrat_10_pl.c"
        "fonts/lv_font_montserrat_12_pl.c"
        "fonts/lv_font_montserrat_14_pl.c"
        "fonts/lv_font_montserrat_18_pl.c"
        "fonts/lv_font_montserrat_24_pl.c"
        "fonts/lv_font_montserrat_72.c"
        "fonts/lv_font_montserrat_80.c"
        "fonts/lv_font_montserrat_96.c"
        "fonts/lv_font_montserrat_120.c"
        "fonts/ui_fonts.c"
    INCLUDE_DIRS
        "."
        "screens"
        "screensavers"
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
        esp_http_client         # screensaver_dashboard
        esp-tls                 # esp_crt_bundle for HTTPS
        buzzer                  # screensaver_dashboard notifications via melodies.h
        mqtt_svc                # screen_mqtt toggle widget
        sdcard                  # screensaver_photo reads slides from SD
        esp_app_format          # esp_app_get_description() — version on splash
)