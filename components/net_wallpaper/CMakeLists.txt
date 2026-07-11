idf_component_register(
    SRCS
        "net_wallpaper.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        lvgl                # lv_image_dsc_t + image cache drop
        esp_http_client
        esp-tls             # esp_crt_bundle for HTTPS sources
        json                # NASA APOD envelope
        libjpeg             # vendored IJG decoder (progressive-capable)
        app_state           # radio playing? capture index before the stop
        services            # radio_stop / radio_play_index around the transfer
)
