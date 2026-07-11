idf_component_register(
    SRCS
        "net_wallpaper.c"
        "net_wallpaper_sched.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        lvgl                # lv_image_dsc_t + image cache drop
        esp_http_client
        esp-tls             # esp_crt_bundle for HTTPS sources
        json                # NASA APOD envelope
        libjpeg             # vendored IJG decoder (progressive-capable)
        app_state           # radio playing? capture index before the stop
        services            # radio_stop / radio_play_index + ntp_service_is_synced
        settings_ex         # scheduler reads wallpaper_url/fetch_mode/hour/min
        sdcard              # save-to-SD writes the fetched image as a .bin
)
