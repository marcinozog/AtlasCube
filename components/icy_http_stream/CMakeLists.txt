idf_component_register(
    SRCS
        "icy_http_stream.c"
        "hls/hls_parse.c"
        "hls/hls_playlist.c"
        "hls/line_reader.c"
        "hls/join_path.c"
    INCLUDE_DIRS
        "include"
    PRIV_INCLUDE_DIRS
        "."
        "hls"
    REQUIRES
        audio_pipeline
        esp_http_client
        esp-tls
)

# ts_demux is shipped as a prebuilt static library (source kept private, see
# lib/). Its undefined audio_calloc/audio_free references resolve here vs esp-adf.
add_prebuilt_library(ts_demux_prebuilt "${CMAKE_SOURCE_DIR}/lib/libts_demux.a")
target_link_libraries(${COMPONENT_LIB} PRIVATE ts_demux_prebuilt)
