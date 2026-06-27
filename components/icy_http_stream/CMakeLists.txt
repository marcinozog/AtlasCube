idf_component_register(
    SRCS
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
    # The prebuilt icy lib (below) calls back into these open hls/ sources.
    # WHOLE_ARCHIVE force-links them so hls_* are always defined regardless of
    # link order, letting the prebuilt's back-references resolve.
    WHOLE_ARCHIVE
)

# Prebuilt static libraries (source kept private, see lib/). The icy lib is named
# libicy_stream_impl.a (NOT libicy_http_stream.a) to avoid colliding with this
# component's own output archive. Linked with --whole-archive so all objects are
# force-included and every symbol is unconditionally present: this resolves the
# mutual ref (icy -> ts_demux_*) AND makes icy_http_stream_* visible to OTHER
# components' prebuilts (e.g. audio_engine -> icy_http_stream_init) regardless of
# link order. esp-adf (audio_element_*, audio_calloc/free) resolves via IDF's
# component link group; hls_* via WHOLE_ARCHIVE above.
add_prebuilt_library(ts_demux_prebuilt "${CMAKE_SOURCE_DIR}/lib/libts_demux.a")
add_prebuilt_library(icy_stream_prebuilt "${CMAKE_SOURCE_DIR}/lib/libicy_stream_impl.a")
target_link_libraries(${COMPONENT_LIB} PRIVATE
    "-Wl,--whole-archive"
    icy_stream_prebuilt
    ts_demux_prebuilt
    "-Wl,--no-whole-archive")
