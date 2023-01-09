add_library(yuzu-android SHARED
    config.cpp
    config.h
    default_ini.h
    emu_window/emu_window.cpp
    emu_window/emu_window.h
    id_cache.cpp
    id_cache.h
    native.cpp
    native.h
)

set_property(TARGET yuzu-android PROPERTY IMPORTED_LOCATION ${FFmpeg_LIBRARY_DIR})

target_link_libraries(yuzu-android PRIVATE audio_core common core input_common)
target_link_libraries(yuzu-android PRIVATE android camera2ndk EGL glad inih jnigraphics log)

set(CPACK_PACKAGE_EXECUTABLES ${CPACK_PACKAGE_EXECUTABLES} yuzu-android)
