idf_component_register(
    SRCS "http_server.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_http_server
        audio
        services
        ws
        settings_ex
        json
        network
        ui
        playlist
        events
)