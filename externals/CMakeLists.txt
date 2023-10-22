# SPDX-FileCopyrightText: 2016 Citra Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

# Dynarmic has cmake_minimum_required(3.12) and we may want to override
# some of its variables, which is only possible in 3.13+
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

# Disable tests in all externals supporting the standard option name
set(BUILD_TESTING OFF)

# Build only static externals
set(BUILD_SHARED_LIBS OFF)

# Skip install rules for all externals
set_directory_properties(PROPERTIES EXCLUDE_FROM_ALL ON)

# xbyak
if ((ARCHITECTURE_x86 OR ARCHITECTURE_x86_64) AND NOT TARGET xbyak::xbyak)
    add_subdirectory(xbyak)
endif()

# Dynarmic
if ((ARCHITECTURE_x86_64 OR ARCHITECTURE_arm64) AND NOT TARGET dynarmic::dynarmic)
    set(DYNARMIC_IGNORE_ASSERTS ON)
    add_subdirectory(dynarmic)
    add_library(dynarmic::dynarmic ALIAS dynarmic)
endif()

# getopt
if (MSVC)
    add_subdirectory(getopt)
endif()

# Glad
add_subdirectory(glad)

# inih
if (NOT TARGET inih::INIReader)
    add_subdirectory(inih)
endif()

# mbedtls
add_subdirectory(mbedtls)
target_include_directories(mbedtls PUBLIC ./mbedtls/include)
if (NOT MSVC)
    target_compile_options(mbedcrypto PRIVATE
        -Wno-unused-but-set-variable
        -Wno-string-concatenation)
endif()

# MicroProfile
add_library(microprofile INTERFACE)
target_include_directories(microprofile INTERFACE ./microprofile)

# GCC bugs
if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "12" AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND MINGW)
    target_compile_options(microprofile INTERFACE "-Wno-array-bounds")
endif()

# libusb
if (ENABLE_LIBUSB AND NOT TARGET libusb::usb)
    add_subdirectory(libusb)
endif()

# SDL2
if (YUZU_USE_EXTERNAL_SDL2)
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

    add_subdirectory(SDL)
endif()

# ENet
if (NOT TARGET enet::enet)
    add_subdirectory(enet)
    target_include_directories(enet INTERFACE ./enet/include)
    add_library(enet::enet ALIAS enet)
endif()

# Cubeb
if (ENABLE_CUBEB AND NOT TARGET cubeb::cubeb)
    set(BUILD_TESTS OFF)
    set(BUILD_TOOLS OFF)
    add_subdirectory(cubeb)
    add_library(cubeb::cubeb ALIAS cubeb)
    if (NOT MSVC)
        if (TARGET speex)
            target_compile_options(speex PRIVATE -Wno-sign-compare)
        endif()
        target_compile_options(cubeb PRIVATE -Wno-implicit-const-int-float-conversion)
    endif()
endif()

# DiscordRPC
if (USE_DISCORD_PRESENCE AND NOT TARGET DiscordRPC::discord-rpc)
    set(BUILD_EXAMPLES OFF)
    add_subdirectory(discord-rpc)
    target_include_directories(discord-rpc INTERFACE ./discord-rpc/include)
    add_library(DiscordRPC::discord-rpc ALIAS discord-rpc)
endif()

# Sirit
add_subdirectory(sirit)

# httplib
if (ENABLE_WEB_SERVICE AND NOT TARGET httplib::httplib)
    set(HTTPLIB_REQUIRE_OPENSSL ON)
    add_subdirectory(cpp-httplib)
endif()

# cpp-jwt
if (ENABLE_WEB_SERVICE AND NOT TARGET cpp-jwt::cpp-jwt)
    set(CPP_JWT_BUILD_EXAMPLES OFF)
    set(CPP_JWT_BUILD_TESTS OFF)
    set(CPP_JWT_USE_VENDORED_NLOHMANN_JSON OFF)
    add_subdirectory(cpp-jwt)
endif()

# Opus
if (NOT TARGET Opus::opus)
    set(OPUS_BUILD_TESTING OFF)
    set(OPUS_BUILD_PROGRAMS OFF)
    set(OPUS_INSTALL_PKG_CONFIG_MODULE OFF)
    set(OPUS_INSTALL_CMAKE_CONFIG_MODULE OFF)
    add_subdirectory(opus)
endif()

# FFMpeg
if (YUZU_USE_BUNDLED_FFMPEG)
    add_subdirectory(ffmpeg)
    set(FFmpeg_PATH "${FFmpeg_PATH}" PARENT_SCOPE)
    set(FFmpeg_LDFLAGS "${FFmpeg_LDFLAGS}" PARENT_SCOPE)
    set(FFmpeg_LIBRARIES "${FFmpeg_LIBRARIES}" PARENT_SCOPE)
    set(FFmpeg_INCLUDE_DIR "${FFmpeg_INCLUDE_DIR}" PARENT_SCOPE)
endif()

# Vulkan-Headers
if (YUZU_USE_EXTERNAL_VULKAN_HEADERS)
    add_subdirectory(Vulkan-Headers)
endif()

# TZDB (Time Zone Database)
add_subdirectory(nx_tzdb)

# VMA
if (NOT TARGET GPUOpen::VulkanMemoryAllocator)
    add_subdirectory(VulkanMemoryAllocator)
endif()

if (NOT TARGET LLVM::Demangle)
    add_library(demangle demangle/ItaniumDemangle.cpp)
    target_include_directories(demangle PUBLIC ./demangle)
    if (NOT MSVC)
        target_compile_options(demangle PRIVATE -Wno-deprecated-declarations) # std::is_pod
    endif()
    add_library(LLVM::Demangle ALIAS demangle)
endif()

add_library(stb stb/stb_dxt.cpp stb/stb_image.cpp stb/stb_image_resize.cpp)
target_include_directories(stb PUBLIC ./stb)

add_library(bc_decoder bc_decoder/bc_decoder.cpp)
target_include_directories(bc_decoder PUBLIC ./bc_decoder)

if (NOT TARGET RenderDoc::API)
    add_library(renderdoc INTERFACE)
    target_include_directories(renderdoc SYSTEM INTERFACE ./renderdoc)
    add_library(RenderDoc::API ALIAS renderdoc)
endif()

if (ANDROID)
   if (ARCHITECTURE_arm64)
       add_subdirectory(libadrenotools)
   endif()
endif()
