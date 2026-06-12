idf_component_register(
    SRCS "rgb_led.c"
    INCLUDE_DIRS "."
    REQUIRES
        driver
        esp_timer
        freertos
)
