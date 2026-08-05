idf_component_register(
    SRCS "espnow_link.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_wifi
        esp_app_format      # esp_app_get_description() for the t:"pong" version
        nvs_flash
        json
        app_state
        services
        settings_ex
        playlist
        network
        audio
        diag                # diag_collect() for t:"diag" — same snapshot as /api/diag
)
