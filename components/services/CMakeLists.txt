idf_component_register(
    SRCS 
        "radio_service.c"
         "ntp_service.c"  
        # "service_events.c"   # odkomentujesz jak zaczniesz używać
    INCLUDE_DIRS 
        "."
    REQUIRES 
        app_state
        ui
        audio
        esp_netif
        lwip
        playlist 
)