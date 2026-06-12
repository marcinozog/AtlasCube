idf_component_register(
    SRCS "battery.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_adc
        esp_hw_support
        freertos
)
