add_library(audio_core STATIC
    audio_out.cpp
    audio_out.h
    buffer.h
    null_sink.h
    stream.cpp
    stream.h
    sink.h
    sink_details.cpp
    sink_details.h
    sink_stream.h
)

create_target_directory_groups(audio_core)

target_link_libraries(audio_core PUBLIC common core)
