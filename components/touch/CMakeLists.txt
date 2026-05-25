set(DRIVERS "")

if(CONFIG_TOUCH_CST816D)
    list(APPEND DRIVERS "drivers/cst816d.c")
endif()

if(CONFIG_TOUCH_FT6336U)
    list(APPEND DRIVERS "drivers/ft6336u.c")
endif()

idf_component_register(
    SRCS
        "touch.c"
        ${DRIVERS}
    INCLUDE_DIRS
        "."
        "drivers"
    REQUIRES
        driver
        lvgl
        ui
)
