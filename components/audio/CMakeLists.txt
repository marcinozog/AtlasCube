idf_component_register(
    SRCS
        "audio_net_player.c"
        "audio_file_player.c"
        "audio_levels.c"
        "id3.c"
        "heap_report.c"
    INCLUDE_DIRS "."
    REQUIRES
        audio_pipeline
        audio_stream
        icy_http_stream
        # esp-adf-libs
        esp_http_client
        app_state
    # The prebuilt audio libs (below) call back into these open sources
    # (heap_report, audio_levels). WHOLE_ARCHIVE force-links them so those
    # symbols are always defined regardless of link order.
    WHOLE_ARCHIVE
)

# Prebuilt static libraries (source kept private, see lib/). Linked with
# --whole-archive so ALL their objects are force-included and every symbol is
# unconditionally present — this resolves both the mutual ref (audio_engine ->
# audio_dsp_*) and cross-component refs INTO these libs (e.g. audio_engine ->
# icy_http_stream_*, whose impl is itself a prebuilt in another component)
# regardless of link order. --start-group is NOT enough for prebuilt->prebuilt
# across components, since imported libs are outside IDF's component link group.
# heap_report / audio_levels_* (open back-refs) resolve via WHOLE_ARCHIVE above.
add_prebuilt_library(audio_dsp_prebuilt    "${CMAKE_SOURCE_DIR}/lib/libaudio_dsp.a")
add_prebuilt_library(audio_engine_prebuilt "${CMAKE_SOURCE_DIR}/lib/libaudio_engine.a")
target_link_libraries(${COMPONENT_LIB} PRIVATE
    "-Wl,--whole-archive" audio_engine_prebuilt audio_dsp_prebuilt "-Wl,--no-whole-archive")