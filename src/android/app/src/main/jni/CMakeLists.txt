# SPDX-FileCopyrightText: 2023 yuzu Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

add_library(yuzu-android SHARED
    android_common/android_common.cpp
    android_common/android_common.h
    applets/software_keyboard.cpp
    applets/software_keyboard.h
    emu_window/emu_window.cpp
    emu_window/emu_window.h
    id_cache.cpp
    id_cache.h
    native.cpp
    native.h
    native_config.cpp
    android_settings.cpp
    game_metadata.cpp
    native_log.cpp
    android_config.cpp
    android_config.h
)

set_property(TARGET yuzu-android PROPERTY IMPORTED_LOCATION ${FFmpeg_LIBRARY_DIR})

target_link_libraries(yuzu-android PRIVATE audio_core common core input_common frontend_common)
target_link_libraries(yuzu-android PRIVATE android camera2ndk EGL glad jnigraphics log)
if (ARCHITECTURE_arm64)
    target_link_libraries(yuzu-android PRIVATE adrenotools)
endif()

set(CPACK_PACKAGE_EXECUTABLES ${CPACK_PACKAGE_EXECUTABLES} yuzu-android)
