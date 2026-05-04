idf_component_register(
    SRCS "encoder.c"
    INCLUDE_DIRS "."
    REQUIRES
        driver
        esp_timer
        freertos
        ui
        app_state
)