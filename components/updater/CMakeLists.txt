# Auto-update component: public interface + FW_VARIANT accessor here, real logic
# in the prebuilt lib/libupdater_impl.a (source private under TMP/private/updater/).
#
# NO --whole-archive here (unlike audio/icy): the prebuilt takes no cross-prebuilt
# back-references — the firmware (main) calls updater_start()/updater_apply()
# directly and passes the variant in as an argument, so both symbols resolve by
# normal archive scanning. Adding a third raw --whole-archive/--no-whole-archive
# pair here breaks CMake's flag ordering and un-force-links icy's prebuilt.
# REQUIRES cover the IDF libs the PREBUILT calls into (updater.c itself uses none),
# so the firmware's final link has them in the graph. All are already in the image
# (web/HTTPS use them) — this is belt-and-braces for link order.
idf_component_register(
    SRCS "updater.c"
    INCLUDE_DIRS "include"
    REQUIRES
        esp_http_client
        esp-tls
        mbedtls
        esp_app_format
        esp_hw_support
        esp_https_ota
        app_update
)

add_prebuilt_library(updater_impl_prebuilt "${CMAKE_SOURCE_DIR}/lib/libupdater_impl.a")
target_link_libraries(${COMPONENT_LIB} PRIVATE updater_impl_prebuilt)
