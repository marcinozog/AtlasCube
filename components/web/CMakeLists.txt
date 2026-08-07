idf_component_register(
    SRCS "http_server.c"
    INCLUDE_DIRS "."
    EMBED_FILES "setup.html"
    REQUIRES
        esp_http_server
        esp_http_client
        esp-tls
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
        espnow_link         # /api/espnow — pairing window for the hardware pilot
        sdcard
        spiffs
        fatfs
        board_pins
        secrets
        updater             # updater_www_stale() — shared web-UI staleness check
        diag                # /api/diag — snapshot shared with the Diagnostics screen
        net_wallpaper       # /api/wallpaper endpoints
        weather
        trace               # /api/settings "trace" — runtime diagnostic-log flags
)
