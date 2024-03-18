# SPDX-FileCopyrightText: 2023 suyu Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

add_library(suyu-android SHARED
    emu_window/emu_window.cpp
    emu_window/emu_window.h
    native.cpp
    native.h
    native_config.cpp
    android_settings.cpp
    game_metadata.cpp
    native_log.cpp
    android_config.cpp
    android_config.h
    native_input.cpp
)

set_property(TARGET suyu-android PROPERTY IMPORTED_LOCATION ${FFmpeg_LIBRARY_DIR})

target_link_libraries(suyu-android PRIVATE audio_core common core input_common frontend_common Vulkan::Headers)
target_link_libraries(suyu-android PRIVATE android camera2ndk EGL glad jnigraphics log)
if (ARCHITECTURE_arm64)
    target_link_libraries(suyu-android PRIVATE adrenotools)
endif()

set(CPACK_PACKAGE_EXECUTABLES ${CPACK_PACKAGE_EXECUTABLES} suyu-android)
