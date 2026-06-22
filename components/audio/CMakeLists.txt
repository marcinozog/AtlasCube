idf_component_register(
    SRCS 
        "audio_engine.c"
        "audio_net_player.c"
        "audio_file_player.c"
        "audio_dsp.c"
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