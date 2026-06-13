idf_component_register(
    SRCS
        "bt.c"
        "bt_qcc5125v2.c"
    INCLUDE_DIRS "."
    REQUIRES
        driver
        audio_board
        ws
)