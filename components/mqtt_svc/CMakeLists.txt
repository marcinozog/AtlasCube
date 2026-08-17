idf_component_register(
    SRCS
        "mqtt_svc.c"
        "mqtt_config.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        mqtt
        json
        app_state
        services
        settings_ex
        audio
        playlist
        spiffs
        secrets
        esp_hw_support      # esp_read_mac — MAC tail in the default client_id
)
