idf_component_register(
    SRCS
        "audio_engine.c"
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
)

# audio_dsp is shipped as a prebuilt static library (source kept private, see
# lib/). Its undefined audio_element_* / audio_levels_push references resolve here.
add_prebuilt_library(audio_dsp_prebuilt "${CMAKE_SOURCE_DIR}/lib/libaudio_dsp.a")
target_link_libraries(${COMPONENT_LIB} PRIVATE audio_dsp_prebuilt)