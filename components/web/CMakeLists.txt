idf_component_register(
    SRCS "http_server.c"
    INCLUDE_DIRS "."
    EMBED_FILES "setup.html"
    REQUIRES
        esp_http_server
        app_update
        esp_partition
        audio
        services
        ws
        settings_ex
        json
        network
        ui
        playlist
        events
        espressif__zlib
        mqtt_svc
        sdcard
        spiffs
        fatfs
        board_pins
        secrets
        updater             # updater_www_stale() — shared web-UI staleness check
        net_wallpaper       # /api/wallpaper endpoints
)