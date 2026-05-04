idf_component_register(
    SRCS "ws_server.c"
    INCLUDE_DIRS "."
    REQUIRES 
        esp_http_server
        services
        json
        app_state
        settings_ex
        playlist
)