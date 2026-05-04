idf_component_register(
    SRCS "bt.c"
    INCLUDE_DIRS "."
    REQUIRES
        driver
        audio_board
        ws
)