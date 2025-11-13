# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# SPDX-FileCopyrightText: 2016 Citra Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

# TODO(crueter): A lot of this should be moved to the root.
# otherwise we have to do weird shenanigans with library linking and stuff

include(CPMUtil)

# Explicitly declare this option here to propagate to the oaknut CPM call
option(DYNARMIC_TESTS "Build tests" ${BUILD_TESTING})

# Dynarmic has cmake_minimum_required(3.12) and we may want to override
# some of its variables, which is only possible in 3.13+
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

# Disable tests/tools in all externals supporting the standard option name
set(BUILD_TESTING OFF)

# Build only static externals
set(BUILD_SHARED_LIBS OFF)

# Skip install rules for all externals
set_directory_properties(PROPERTIES EXCLUDE_FROM_ALL ON)

# Xbyak (also used by Dynarmic, so needs to be added first)
if (ARCHITECTURE_x86 OR ARCHITECTURE_x86_64)
    if (PLATFORM_SUN OR PLATFORM_OPENBSD)
        AddJsonPackage(xbyak_sun)
    else()
        AddJsonPackage(xbyak)
    endif()
endif()

# Oaknut (also used by Dynarmic, so needs to be added first)
if (ARCHITECTURE_arm64 OR DYNARMIC_TESTS)
    AddJsonPackage(oaknut)
endif()

# enet
AddJsonPackage(enet)

if (enet_ADDED)
    target_include_directories(enet INTERFACE ${enet_SOURCE_DIR}/include)
endif()

if (NOT TARGET enet::enet)
    add_library(enet::enet ALIAS enet)
endif()

# mbedtls
AddJsonPackage(mbedtls)

# VulkanUtilityHeaders - pulls in headers and utility libs
AddJsonPackage(vulkan-utility-headers)

# small hack
if (NOT VulkanUtilityLibraries_ADDED)
    find_package(VulkanHeaders 1.3.274 REQUIRED)
endif()

# DiscordRPC
if (USE_DISCORD_PRESENCE)
    if (ARCHITECTURE_arm64)
        add_compile_definitions(RAPIDJSON_ENDIAN=RAPIDJSON_LITTLEENDIAN)
    endif()

    AddJsonPackage(discord-rpc)

    if (DiscordRPC_ADDED)
        target_include_directories(discord-rpc INTERFACE ${DiscordRPC_SOURCE_DIR}/include)
        add_library(DiscordRPC::discord-rpc ALIAS discord-rpc)
    endif()
endif()

# SimpleIni
AddJsonPackage(simpleini)

# Most linux distros don't package cubeb, so enable regardless of cpm settings
if(ENABLE_CUBEB)
    AddJsonPackage(cubeb)

    if (cubeb_ADDED)
        if (NOT MSVC)
            if (TARGET speex)
                target_compile_options(speex PRIVATE -Wno-sign-compare)
            endif()

            set_target_properties(cubeb PROPERTIES COMPILE_OPTIONS "")
            target_compile_options(cubeb INTERFACE
                -Wno-implicit-const-int-float-conversion
                -Wno-shadow
                -Wno-missing-declarations
                -Wno-return-type
                -Wno-uninitialized
            )
        else()
            target_compile_options(cubeb PRIVATE
                /wd4456
                /wd4458
            )
        endif()
    endif()

    if (NOT TARGET cubeb::cubeb)
        add_library(cubeb::cubeb ALIAS cubeb)
    endif()
endif()

# find SDL2 exports a bunch of variables that are needed, so its easier to do this outside of the YUZU_find_package
if (ENABLE_SDL2)
    if (YUZU_USE_EXTERNAL_SDL2)
        message(STATUS "Using SDL2 from externals.")
        if (NOT WIN32)
            # Yuzu itself needs: Atomic Audio Events Joystick Haptic Sensor Threads Timers
            # Since 2.0.18 Atomic+Threads required for HIDAPI/libusb (see https://github.com/libsdl-org/SDL/issues/5095)
            # Yuzu-cmd also needs: Video (depends on Loadso/Dlopen)
            # CPUinfo also required for SDL Audio, at least until 2.28.0 (see https://github.com/libsdl-org/SDL/issues/7809)
            set(SDL_UNUSED_SUBSYSTEMS
                File Filesystem
                Locale Power Render)
            foreach(_SUB ${SDL_UNUSED_SUBSYSTEMS})
              string(TOUPPER ${_SUB} _OPT)
              set(SDL_${_OPT} OFF)
            endforeach()

            set(HIDAPI ON)
        endif()

        if (APPLE)
            set(SDL_FILE ON)
        endif()

        if ("${YUZU_SYSTEM_PROFILE}" STREQUAL "steamdeck")
            set(SDL_PIPEWIRE OFF) # build errors out with this on
            AddJsonPackage("sdl2_steamdeck")
        else()
            AddJsonPackage("sdl2_generic")
        endif()
    elseif (YUZU_USE_BUNDLED_SDL2)
        message(STATUS "Using bundled SDL2")
        if (PLATFORM_FREEBSD)
            set(BUILD_SHARED_LIBS ON)
        endif()
        AddJsonPackage(sdl2)
    endif()

    find_package(SDL2 2.26.4 REQUIRED)
endif()

set(BUILD_SHARED_LIBS OFF)

# SPIRV Headers
AddJsonPackage(spirv-headers)

# Sirit
if (YUZU_USE_BUNDLED_SIRIT)
    AddJsonPackage(sirit-ci)
else()
    AddJsonPackage(sirit)
    if(MSVC AND CXX_CLANG)
        target_compile_options(siritobj PRIVATE -Wno-error=unused-command-line-argument)
    endif()
endif()

# SPIRV Tools
AddJsonPackage(spirv-tools)

if (SPIRV-Tools_ADDED)
    add_library(SPIRV-Tools::SPIRV-Tools ALIAS SPIRV-Tools-static)
    target_link_libraries(SPIRV-Tools-static PRIVATE SPIRV-Tools-opt SPIRV-Tools-link)
endif()

# Catch2
if (YUZU_TESTS OR DYNARMIC_TESTS)
    AddJsonPackage(catch2)
endif()

# getopt
if (MSVC)
    add_subdirectory(getopt)
endif()

# Glad
add_subdirectory(glad)

# libusb
if (ENABLE_LIBUSB)
    add_subdirectory(libusb)
endif()

# VMA
AddJsonPackage(vulkan-memory-allocator)

if (VulkanMemoryAllocator_ADDED)
    if (CXX_CLANG)
        target_compile_options(VulkanMemoryAllocator INTERFACE
            -Wno-unused-variable
        )
    elseif(MSVC)
        target_compile_options(VulkanMemoryAllocator INTERFACE
            /wd4189
        )
    endif()
endif()

# httplib
if (ENABLE_WEB_SERVICE OR ENABLE_UPDATE_CHECKER)
    AddJsonPackage(httplib)
endif()

# cpp-jwt
if (ENABLE_WEB_SERVICE OR ENABLE_UPDATE_CHECKER)
    AddJsonPackage(cpp-jwt)
endif()

# unordered_dense
AddJsonPackage(unordered-dense)

# FFMpeg
if (YUZU_USE_EXTERNAL_FFMPEG OR YUZU_USE_BUNDLED_FFMPEG)
    add_subdirectory(ffmpeg)

    set(FFmpeg_PATH "${FFmpeg_PATH}" PARENT_SCOPE)
    set(FFmpeg_LDFLAGS "${FFmpeg_LDFLAGS}" PARENT_SCOPE)
    set(FFmpeg_LIBRARIES "${FFmpeg_LIBRARIES}" PARENT_SCOPE)
    set(FFmpeg_LIBRARY_DIR "${FFmpeg_LIBRARY_DIR}" PARENT_SCOPE)
    set(FFmpeg_INCLUDE_DIR "${FFmpeg_INCLUDE_DIR}" PARENT_SCOPE)

    message(STATUS "FFmpeg Libraries: ${FFmpeg_LIBRARIES}")
endif()

# TZDB (Time Zone Database)
add_subdirectory(nx_tzdb)

if (NOT TARGET LLVM::Demangle)
    add_library(demangle demangle/ItaniumDemangle.cpp)
    target_include_directories(demangle PUBLIC ./demangle)
    if (NOT MSVC)
        target_compile_options(demangle PRIVATE -Wno-deprecated-declarations) # std::is_pod
    endif()
    add_library(LLVM::Demangle ALIAS demangle)
endif()

add_library(stb stb/stb_dxt.cpp)
target_include_directories(stb PUBLIC ./stb)

if (NOT TARGET stb::headers)
    add_library(stb::headers ALIAS stb)
endif()

add_library(tz tz/tz/tz.cpp)
target_include_directories(tz PUBLIC ./tz)

add_library(bc_decoder bc_decoder/bc_decoder.cpp)
target_include_directories(bc_decoder PUBLIC ./bc_decoder)

if (NOT TARGET RenderDoc::API)
    add_library(renderdoc INTERFACE)
    target_include_directories(renderdoc SYSTEM INTERFACE ./renderdoc)
    add_library(RenderDoc::API ALIAS renderdoc)
endif()

if (ANDROID AND ARCHITECTURE_arm64)
    AddJsonPackage(libadrenotools)
endif()

if (UNIX AND NOT APPLE AND NOT TARGET gamemode::headers)
    add_library(gamemode INTERFACE)
    target_include_directories(gamemode INTERFACE gamemode)
    add_library(gamemode::headers ALIAS gamemode)
endif()

# Breakpad
# TODO(crueter): Breakpad needs additional fetches
# https://github.com/google/breakpad/blob/main/DEPS
# https://github.com/microsoft/vcpkg/blob/master/ports/breakpad/CMakeLists.txt
if (YUZU_CRASH_DUMPS AND NOT TARGET libbreakpad_client)
    set(BREAKPAD_WIN32_DEFINES
        NOMINMAX
        UNICODE
        WIN32_LEAN_AND_MEAN
        _CRT_SECURE_NO_WARNINGS
        _CRT_SECURE_NO_DEPRECATE
        _CRT_NONSTDC_NO_DEPRECATE
    )

    # TODO
    AddPackage(
        NAME breakpad
        URL "google/breakpad"
        SHA f80f288803
        HASH 4a87ee88cea99bd633d52a5b00135a649f1475e3b65db325a6df9c804ab82b054ad7e62419b35f6e22cc5dfbbb569214041d7ad5d10fab10106e700bb5050e1d
        DOWNLOAD_ONLY YES
    )

    # libbreakpad
    add_library(libbreakpad STATIC)
    file(GLOB_RECURSE LIBBREAKPAD_SOURCES ${breakpad_SOURCE_DIR}/src/processor/*.cc)
    file(GLOB_RECURSE LIBDISASM_SOURCES ${breakpad_SOURCE_DIR}/src/third_party/libdisasm/*.c)
    list(FILTER LIBBREAKPAD_SOURCES EXCLUDE REGEX "_unittest|_selftest|synth_minidump|/tests|/testdata|/solaris|microdump_stackwalk|minidump_dump|minidump_stackwalk")
    if (WIN32)
        list(FILTER LIBBREAKPAD_SOURCES EXCLUDE REGEX "/linux|/mac|/android")
        target_compile_definitions(libbreakpad PRIVATE ${BREAKPAD_WIN32_DEFINES})
        target_include_directories(libbreakpad PRIVATE "${CMAKE_GENERATOR_INSTANCE}/DIA SDK/include")
    elseif (APPLE)
        list(FILTER LIBBREAKPAD_SOURCES EXCLUDE REGEX "/linux|/windows|/android")
    else()
        list(FILTER LIBBREAKPAD_SOURCES EXCLUDE REGEX "/mac|/windows|/android")
    endif()
    target_sources(libbreakpad PRIVATE ${LIBBREAKPAD_SOURCES} ${LIBDISASM_SOURCES})
    target_include_directories(libbreakpad
        PUBLIC
            ${breakpad_SOURCE_DIR}/src
            ${breakpad_SOURCE_DIR}/src/third_party/libdisasm
    )

    # libbreakpad_client
    add_library(libbreakpad_client STATIC)
    file(GLOB LIBBREAKPAD_COMMON_SOURCES ${breakpad_SOURCE_DIR}/src/common/*.cc ${breakpad_SOURCE_DIR}/src/common/*.c ${breakpad_SOURCE_DIR}/src/client/*.cc)

    if (WIN32)
        file(GLOB_RECURSE LIBBREAKPAD_CLIENT_SOURCES ${breakpad_SOURCE_DIR}/src/client/windows/*.cc ${breakpad_SOURCE_DIR}/src/common/windows/*.cc)
        list(FILTER LIBBREAKPAD_COMMON_SOURCES EXCLUDE REGEX "language.cc|path_helper.cc|stabs_to_module.cc|stabs_reader.cc|minidump_file_writer.cc")
        target_include_directories(libbreakpad_client PRIVATE "${CMAKE_GENERATOR_INSTANCE}/DIA SDK/include")
        target_compile_definitions(libbreakpad_client PRIVATE ${BREAKPAD_WIN32_DEFINES})
    elseif (APPLE)
        target_compile_definitions(libbreakpad_client PRIVATE HAVE_MACH_O_NLIST_H)
        file(GLOB_RECURSE LIBBREAKPAD_CLIENT_SOURCES ${breakpad_SOURCE_DIR}/src/client/mac/*.cc ${breakpad_SOURCE_DIR}/src/common/mac/*.cc)
        list(APPEND LIBBREAKPAD_CLIENT_SOURCES ${breakpad_SOURCE_DIR}/src/common/mac/MachIPC.mm)
    else()
        target_compile_definitions(libbreakpad_client PUBLIC HAVE_A_OUT_H)
        file(GLOB_RECURSE LIBBREAKPAD_CLIENT_SOURCES ${breakpad_SOURCE_DIR}/src/client/linux/*.cc ${breakpad_SOURCE_DIR}/src/common/linux/*.cc)
    endif()
    list(APPEND LIBBREAKPAD_CLIENT_SOURCES ${LIBBREAKPAD_COMMON_SOURCES})
    list(FILTER LIBBREAKPAD_CLIENT_SOURCES EXCLUDE REGEX "/sender|/tests|/unittests|/testcases|_unittest|_test")
    target_sources(libbreakpad_client PRIVATE ${LIBBREAKPAD_CLIENT_SOURCES})
    target_include_directories(libbreakpad_client PRIVATE ${breakpad_SOURCE_DIR}/src)

    if (WIN32)
        target_link_libraries(libbreakpad_client PRIVATE wininet.lib)
    elseif (APPLE)
        find_library(CoreFoundation_FRAMEWORK CoreFoundation)
        target_link_libraries(libbreakpad_client PRIVATE ${CoreFoundation_FRAMEWORK})
    else()
        find_library(PTHREAD_LIBRARIES pthread)
        target_compile_definitions(libbreakpad_client PRIVATE HAVE_GETCONTEXT=1)
        if (PTHREAD_LIBRARIES)
            target_link_libraries(libbreakpad_client PRIVATE ${PTHREAD_LIBRARIES})
        endif()
    endif()

    # Host tools for symbol processing
    if (LINUX)
        find_package(ZLIB REQUIRED)

        add_executable(minidump_stackwalk ${breakpad_SOURCE_DIR}/src/processor/minidump_stackwalk.cc)
        target_link_libraries(minidump_stackwalk PRIVATE libbreakpad libbreakpad_client)

        add_executable(dump_syms
            ${breakpad_SOURCE_DIR}/src/common/dwarf_cfi_to_module.cc
            ${breakpad_SOURCE_DIR}/src/common/dwarf_cu_to_module.cc
            ${breakpad_SOURCE_DIR}/src/common/dwarf_line_to_module.cc
            ${breakpad_SOURCE_DIR}/src/common/dwarf_range_list_handler.cc
            ${breakpad_SOURCE_DIR}/src/common/language.cc
            ${breakpad_SOURCE_DIR}/src/common/module.cc
            ${breakpad_SOURCE_DIR}/src/common/path_helper.cc
            ${breakpad_SOURCE_DIR}/src/common/stabs_reader.cc
            ${breakpad_SOURCE_DIR}/src/common/stabs_to_module.cc
            ${breakpad_SOURCE_DIR}/src/common/dwarf/bytereader.cc
            ${breakpad_SOURCE_DIR}/src/common/dwarf/dwarf2diehandler.cc
            ${breakpad_SOURCE_DIR}/src/common/dwarf/dwarf2reader.cc
            ${breakpad_SOURCE_DIR}/src/common/dwarf/elf_reader.cc
            ${breakpad_SOURCE_DIR}/src/common/linux/crc32.cc
            ${breakpad_SOURCE_DIR}/src/common/linux/dump_symbols.cc
            ${breakpad_SOURCE_DIR}/src/common/linux/elf_symbols_to_module.cc
            ${breakpad_SOURCE_DIR}/src/common/linux/elfutils.cc
            ${breakpad_SOURCE_DIR}/src/common/linux/file_id.cc
            ${breakpad_SOURCE_DIR}/src/common/linux/linux_libc_support.cc
            ${breakpad_SOURCE_DIR}/src/common/linux/memory_mapped_file.cc
            ${breakpad_SOURCE_DIR}/src/common/linux/safe_readlink.cc
            ${breakpad_SOURCE_DIR}/src/tools/linux/dump_syms/dump_syms.cc)
        target_link_libraries(dump_syms PRIVATE libbreakpad_client)
    endif()
endif()

# oboe
if (ANDROID)
    AddJsonPackage(oboe)

    add_library(oboe::oboe ALIAS oboe)
endif()
