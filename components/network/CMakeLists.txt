idf_component_register(
    SRCS "mdns_service.c" "wifi_manager.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_wifi
        esp_event
        esp_netif
        nvs_flash
        app_update
        mdns
        settings_ex
)