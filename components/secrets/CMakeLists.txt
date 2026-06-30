idf_component_register(
    SRCS
        "secrets.c"
    INCLUDE_DIRS
        "include"
    REQUIRES
        nvs_flash
)
