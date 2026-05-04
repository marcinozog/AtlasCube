idf_component_register(
    SRCS "app_main.c"
    INCLUDE_DIRS "."
    REQUIRES
        nvs_flash
        audio
        web
        network
        ws
        display
        ui
        settings_ex
        app_state
        bt_module
        services
        encoder
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_CONF_INCLUDE_SIMPLE)