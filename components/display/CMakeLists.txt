set(DRIVERS "")

if(CONFIG_DISPLAY_ILI9341)
    list(APPEND DRIVERS "drivers/ili9341.c")
endif()

if(CONFIG_DISPLAY_ST7796)
    list(APPEND DRIVERS "drivers/st7796.c")
endif()

if(CONFIG_DISPLAY_SSD1322)
    list(APPEND DRIVERS "drivers/ssd1322.c")
endif()

idf_component_register(
    SRCS 
        "display.c"
        ${DRIVERS}
    INCLUDE_DIRS 
        "."
        "drivers"
    REQUIRES
        lvgl
        driver
        audio_board
        esp_lcd
        ui
)