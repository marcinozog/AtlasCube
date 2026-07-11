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
        touch
        mqtt_svc
        sdcard
        board_pins
        updater
        net_wallpaper
)