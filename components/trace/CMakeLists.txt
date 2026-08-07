idf_component_register(
    SRCS "trace.c"
    INCLUDE_DIRS "include"
    REQUIRES
        log         # the TRACE macros expand to ESP_LOGI in every consumer
        esp_timer   # TRACE_EVERY_MS throttle
)
