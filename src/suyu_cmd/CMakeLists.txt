# SPDX-FileCopyrightText: 2018 yuzu Emulator Project
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

if (SUYU_CMD OR YUZU_CMD)
    set(SUYU_CMD_SOURCES
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

    add_executable(suyu-cmd ${SUYU_CMD_SOURCES})
    set(SUYU_CMD_TARGETS suyu-cmd)

    # ---------------------------------------------------------------------
    # Per-game statically recompiled variant.
    #
    # SUYU_CMD_RECOMP_DIR points at one export's exefs/ directory, which holds
    # a generated recomp_registration.c plus one subdirectory per NSO module
    # with a generated CMakeLists.txt. Each of those contributes a static
    # library (recomp_static_<module>) whose C symbols have been renamed per
    # module, so they can all live in one binary. The result is a single
    # self-contained executable: no recompiled_*.dll to ship beside it.
    #
    # This is an *additional* target - plain suyu-cmd stays game-agnostic.
    # ---------------------------------------------------------------------
    if (SUYU_CMD_RECOMP_DIR AND EXISTS "${SUYU_CMD_RECOMP_DIR}/recomp_registration.c")
        enable_language(C)
        # Tells the generated module projects to emit only the static library,
        # skipping their portable standalone exe (whose target name would
        # collide between modules) and their loadable shared image.
        set(RECOMP_STATIC_ONLY ON)
        set(_recomp_libs "")
        # file(GLOB) treats [ ] * ? in the WHOLE path as glob syntax, not just
        # the trailing "/*" - a title whose exported folder name carries its
        # title ID in brackets (e.g. "Pokemon Sword[0100ABF008968000][US][v0]",
        # produced whenever a ROM has no NACP title to name the folder from)
        # silently matches nothing, so every module looks missing even though
        # recomp_export.c/CMakeLists.txt are sitting right there on disk.
        # Escaping each special character to its bracket-literal form before
        # globbing keeps this working for any export path, not just ones that
        # happen to avoid those characters.
        # Order matters: each replacement's own output must not be re-mangled
        # by a later replacement, so "[" goes through a placeholder that
        # contains no bracket characters until every other substitution has
        # already run.
        set(_recomp_dir_escaped "${SUYU_CMD_RECOMP_DIR}")
        string(REPLACE "[" "@LBRACKET@" _recomp_dir_escaped "${_recomp_dir_escaped}")
        string(REPLACE "]" "[]]" _recomp_dir_escaped "${_recomp_dir_escaped}")
        string(REPLACE "*" "[*]" _recomp_dir_escaped "${_recomp_dir_escaped}")
        string(REPLACE "?" "[?]" _recomp_dir_escaped "${_recomp_dir_escaped}")
        string(REPLACE "@LBRACKET@" "[[]" _recomp_dir_escaped "${_recomp_dir_escaped}")
        file(GLOB _recomp_children RELATIVE "${SUYU_CMD_RECOMP_DIR}"
             "${_recomp_dir_escaped}/*")
        foreach(_m ${_recomp_children})
            if (EXISTS "${SUYU_CMD_RECOMP_DIR}/${_m}/CMakeLists.txt" AND
                EXISTS "${SUYU_CMD_RECOMP_DIR}/${_m}/recomp_export.c")
                add_subdirectory("${SUYU_CMD_RECOMP_DIR}/${_m}"
                                 "${CMAKE_CURRENT_BINARY_DIR}/recomp/${_m}")
                list(APPEND _recomp_libs "recomp_static_${_m}")
            endif()
        endforeach()
        if (_recomp_libs)
            add_executable(suyu-cmd-static ${SUYU_CMD_SOURCES}
                           "${SUYU_CMD_RECOMP_DIR}/recomp_registration.c")
            target_compile_definitions(suyu-cmd-static PRIVATE SUYU_CMD_STATIC_RECOMP=1)
            target_link_libraries(suyu-cmd-static PRIVATE ${_recomp_libs})
            list(APPEND SUYU_CMD_TARGETS suyu-cmd-static)
        else()
            message(WARNING "SUYU_CMD_RECOMP_DIR set but no recompiled modules found in it")
        endif()
    endif()

    create_resource("../../dist/suyu.bmp" "suyu_cmd/suyu_icon.h" "suyu_icon")

    foreach(_t ${SUYU_CMD_TARGETS})
        if (APPLE)
            target_sources(${_t} PRIVATE
                emu_window/emu_window_sdl2_mtl.cpp
                emu_window/emu_window_sdl2_mtl.h
            )
        endif()

        target_link_libraries(${_t} PRIVATE common core input_common frontend_common)
        target_link_libraries(${_t} PRIVATE glad)
        if (MSVC)
            target_link_libraries(${_t} PRIVATE getopt)
        endif()
        target_link_libraries(${_t} PRIVATE ${PLATFORM_LIBRARIES} Threads::Threads)

        target_include_directories(${_t} PRIVATE ${RESOURCES_DIR})

        target_link_libraries(${_t} PRIVATE SDL3::SDL3 Vulkan::Headers VulkanMemoryAllocator)
        # FFmpeg headers are not transitively exposed by core; add them explicitly
        target_include_directories(${_t} PRIVATE ${FFmpeg_INCLUDE_DIR})

        if(UNIX)
            if (APPLE)
                target_link_libraries(${_t} PRIVATE metal-cpp)  # Metal support for macOS
            endif()
        endif()

        if(WIN32)
            # compile as a win32 gui application instead of a console application
            if(MSVC)
                set_target_properties(${_t} PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
            elseif(MINGW)
                set_target_properties(${_t} PROPERTIES LINK_FLAGS_RELEASE "-Wl,--subsystem,windows")
            endif()
            target_link_libraries(${_t} PRIVATE ${PLATFORM_LIBRARIES} Threads::Threads)  # Fixed typo: yuzu-cmd -> suyu-cmd
        endif()

        if (MSVC AND SUYU_USE_BUNDLED_SDL3)
            include(CopySuyuSDLDeps)
            copy_suyu_SDL_deps(${_t})
        endif()

        if (SUYU_USE_PRECOMPILED_HEADERS)
            target_precompile_headers(${_t} PRIVATE precompiled_headers.h)
        endif()

        create_target_directory_groups(${_t})
    endforeach()

    if(UNIX AND NOT APPLE)
        install(TARGETS suyu-cmd)
    endif()
endif()