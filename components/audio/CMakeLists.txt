idf_component_register(
    SRCS 
        "audio_player.c"
        "audio_dsp.c"
    INCLUDE_DIRS "."
    REQUIRES
        audio_pipeline
        audio_stream
        icy_http_stream
        # esp-adf-libs
        esp_http_client
        settings_ex
)