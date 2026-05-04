idf_component_register(
    SRCS "settings.c"
    INCLUDE_DIRS "."
    REQUIRES
        json
        audio
        app_state
        bt_module
        display
)