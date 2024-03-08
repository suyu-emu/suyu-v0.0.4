# SPDX-FileCopyrightText: 2018 yuzu Emulator Project & 2024 suyu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

# Credits to Samantas5855 and others for this function.
function(create_resource file output filename)
    # Read hex data from file
    file(READ ${file} filedata HEX)
    # Convert hex data for C compatibility
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," filedata ${filedata})
    # Write data to output file
    set(RESOURCES_DIR "${PROJECT_BINARY_DIR}/dist" PARENT_SCOPE)
    file(WRITE "${PROJECT_BINARY_DIR}/dist/${output}" "const unsigned char ${filename}[] = {${filedata}};\nconst unsigned ${filename}_size = sizeof(${filename});\n")
endfunction()

add_executable(suyu-cmd
    emu_window/emu_window_sdl2.cpp
    emu_window/emu_window_sdl2.h
    emu_window/emu_window_sdl2_gl.cpp
    emu_window/emu_window_sdl2_gl.h
    emu_window/emu_window_sdl2_null.cpp
    emu_window/emu_window_sdl2_null.h
    emu_window/emu_window_sdl2_vk.cpp
    emu_window/emu_window_sdl2_vk.h
    precompiled_headers.h
    sdl_config.cpp
    sdl_config.h
    suyu.cpp
    suyu.rc
)

target_link_libraries(suyu-cmd PRIVATE common core input_common frontend_common)
target_link_libraries(suyu-cmd PRIVATE glad)
if (MSVC)
    target_link_libraries(suyu-cmd PRIVATE getopt)
endif()
target_link_libraries(suyu-cmd PRIVATE ${PLATFORM_LIBRARIES} Threads::Threads)

create_resource("../../dist/suyu.bmp" "suyu_cmd/suyu_icon.h" "suyu_icon")
target_include_directories(suyu-cmd PRIVATE ${RESOURCES_DIR})

target_link_libraries(suyu-cmd PRIVATE SDL2::SDL2 Vulkan::Headers)

if(UNIX AND NOT APPLE)
    install(TARGETS suyu-cmd)
endif()

if(WIN32)
    # compile as a win32 gui application instead of a console application
    if(MSVC)
        set_target_properties(suyu-cmd PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
    elseif(MINGW)
        set_target_properties(suyu-cmd PROPERTIES LINK_FLAGS_RELEASE "-Wl,--subsystem,windows")
    endif()
endif()

if (MSVC)
    include(CopySuyuSDLDeps)
    copy_suyu_SDL_deps(suyu-cmd)
endif()

if (SUYU_USE_PRECOMPILED_HEADERS)
    target_precompile_headers(suyu-cmd PRIVATE precompiled_headers.h)
endif()

create_target_directory_groups(suyu-cmd)
