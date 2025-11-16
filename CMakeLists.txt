# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.22)

project(yuzu)

if (${CMAKE_SYSTEM_NAME} STREQUAL "SunOS")
    set(PLATFORM_SUN ON)
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
    set(PLATFORM_FREEBSD ON)
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "OpenBSD")
    set(PLATFORM_OPENBSD ON)
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "NetBSD")
    set(PLATFORM_NETBSD ON)
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "DragonFly")
    set(PLATFORM_DRAGONFLYBSD ON)
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Haiku")
    set(PLATFORM_HAIKU ON)
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    set(PLATFORM_LINUX ON)
endif()

# dumb heuristic to detect msys2
if (CMAKE_COMMAND MATCHES "msys64")
    set(PLATFORM_MSYS ON)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CXX_CLANG ON)
    if (MSVC)
        set(CXX_CLANG_CL ON)
    endif()
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CXX_GCC ON)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(CXX_CL ON)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "IntelLLVM")
    set(CXX_ICC ON)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    set(CXX_APPLE ON)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/externals/cmake-modules")

# https://gitlab.kitware.com/cmake/cmake/-/merge_requests/11112
# This works totally fine on MinGW64, but not CLANG{,ARM}64
if(MINGW AND CXX_CLANG)
    set(CMAKE_SYSTEM_VERSION 10.0.0)
endif()

# NB: this does not account for SPARC
# If you get Eden working on SPARC, please shoot crueter@crueter.xyz multiple emails
# and you will be hailed for eternity
if (PLATFORM_SUN)
    # Terrific Solaris pkg shenanigans
    list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SYSROOT}/usr/lib/qt/6.6/lib/amd64/cmake")
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_SYSROOT}/usr/lib/qt/6.6/lib/amd64/cmake")

    # Amazing - absolutely incredible
    list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SYSROOT}/usr/lib/amd64/cmake")
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_SYSROOT}/usr/lib/amd64/cmake")

    # For some mighty reason, doing a normal release build sometimes may not trigger
    # the proper -O3 switch to materialize
    if (CMAKE_BUILD_TYPE MATCHES "Release")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
    endif()
    if (CMAKE_BUILD_TYPE MATCHES "RelWithDebInfo")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
    endif()
endif()

# Needed for FFmpeg w/ VAAPI and DRM
if (PLATFORM_OPENBSD)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I${CMAKE_SYSROOT}/usr/X11R6/include")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I${CMAKE_SYSROOT}/usr/X11R6/include")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L${CMAKE_SYSROOT}/usr/X11R6/lib")
elseif (PLATFORM_NETBSD)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I${CMAKE_SYSROOT}/usr/X11R7/include")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I${CMAKE_SYSROOT}/usr/X11R7/include")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L${CMAKE_SYSROOT}/usr/X11R7/lib")
endif()

# NetBSD: Fun for the whole family!
if (PLATFORM_NETBSD)
    set(ENV{PKG_CONFIG_PATH} "${PKG_CONFIG_PATH}:${CMAKE_SYSROOT}/usr/pkg/lib/ffmpeg7/pkgconfig")
endif()

# MSYS2 utilities
if (PLATFORM_MSYS)
    include(FixMsysPaths)
    # really, really dumb heuristic to detect what environment we are in
    macro(system var)
        if (CMAKE_COMMAND MATCHES ${var})
            set(MSYSTEM ${var})
        endif()
    endmacro()

    system(mingw64)
    system(clang64)
    system(clangarm64)
    system(ucrt64)

    if (NOT DEFINED MSYSTEM)
        set(MSYSTEM msys2)
    endif()

    # we (generally) want to prioritize environment-specific binaries if possible
    # some, like autoconf, are not present on environments besides msys2 though
    set(CMAKE_PROGRAM_PATH C:/msys64/${MSYSTEM}/bin C:/msys64/usr/bin)
    set(ENV{PKG_CONFIG_PATH} C:/msys64/${MSYSTEM}/lib/pkgconfig)
endif()

# static stuff
option(YUZU_STATIC_BUILD "Use static libraries and executables if available" OFF)

if (YUZU_STATIC_BUILD)
    include(StaticQtLibs)

    # lol
    set(Boost_USE_STATIC_LIBS ON)
    set(BUILD_SHARED_LIBS OFF)

    ## find .a libs first (static, usually)
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")

    ## some libraries define a Library::Name_static alternative ##
    set(YUZU_STATIC_SUFFIX _static)

    ## some libraries use CMAKE_IMPORT_LIBRARY_SUFFIX e.g. Harfbuzz ##
    set(CMAKE_IMPORT_LIBRARY_SUFFIX ".a")

    if (MINGW)
        # simple hook to reject dynamic libs
        function(find_library var)
            # also skip previously-found libraries cuz... yaknow
            # UNLESS they are dll.a :{
            if (${var} AND NOT ${var} MATCHES "dll\\.a$")
                return()
            endif()

            _find_library(${var} ${ARGN})
            if (${var})
                get_filename_component(lib_name "${${var}}" NAME)
                if (lib_name MATCHES "dll\\.a$")
                    unset(${var} CACHE)
                    set(${var} "${var}-NOTFOUND" CACHE INTERNAL "" FORCE)
                endif()
            endif()
        endfunction()

        # msys2 quazip does not build a static lib
        set(QuaZip-Qt6_FORCE_BUNDLED ON)

        set(YUZU_USE_BUNDLED_FFMPEG ON)
        set(YUZU_USE_BUNDLED_SDL2 ON)
        set(YUZU_USE_BUNDLED_OPENSSL ON)

        set(HTTPLIB_USE_BROTLI_IF_AVAILABLE OFF)
    elseif(APPLE)
        # these libs do not properly provide static libs/let you do it with cmake
        set(YUZU_USE_CPM ON)

        set(YUZU_USE_BUNDLED_FFMPEG ON)
        set(YUZU_USE_BUNDLED_SDL2 ON)
        set(YUZU_USE_BUNDLED_OPENSSL ON)

        # IMPORTED_IMPLIB not set for imported target
        # TODO(crueter): wtf
        set(fmt_FORCE_BUNDLED ON)
        set(SPIRV-Tools_FORCE_BUNDLED ON)
        set(SPIRV-Headers_FORCE_BUNDLED ON)
        set(zstd_FORCE_BUNDLED ON)
    endif()
endif()

# Detect current compilation architecture and create standard definitions
# =======================================================================

include(CheckSymbolExists)
function(detect_architecture symbol arch)
    if (NOT DEFINED ARCHITECTURE)
        set(CMAKE_REQUIRED_QUIET 1)
        check_symbol_exists("${symbol}" "" ARCHITECTURE_${arch})
        unset(CMAKE_REQUIRED_QUIET)

        # The output variable needs to be unique across invocations otherwise
        # CMake's crazy scope rules will keep it defined
        if (ARCHITECTURE_${arch})
            set(ARCHITECTURE "${arch}" PARENT_SCOPE)
            set(ARCHITECTURE_${arch} 1 PARENT_SCOPE)
            add_definitions("-DARCHITECTURE_${arch}=1")
        endif()
    endif()
endfunction()

if (NOT ENABLE_GENERIC)
    # https://sourceforge.net/p/predef/wiki/Architectures/
    # TODO: THIS IS FUCKING FLAWED ONLY THE FIRST SYMBOL THAT APPEARS WILL BE CONSIDERED :(
    if (MSVC)
        detect_architecture("_M_AMD64" x86_64)
        detect_architecture("_M_IX86" x86)
        detect_architecture("_M_ARM" arm)
        detect_architecture("_M_ARM64" arm64)
    else()
        detect_architecture("__x86_64__" x86_64)
        detect_architecture("__i386__" x86)
        detect_architecture("__arm__" arm)
        detect_architecture("__aarch64__" arm64)
    endif()
    detect_architecture("__ARM64__" arm64)
    detect_architecture("__aarch64__" arm64)
    detect_architecture("_M_ARM64" arm64)

    detect_architecture("__arm__" arm)
    detect_architecture("__TARGET_ARCH_ARM" arm)
    detect_architecture("_M_ARM" arm)

    detect_architecture("__x86_64" x86_64)
    detect_architecture("__x86_64__" x86_64)
    detect_architecture("__amd64" x86_64)
    detect_architecture("_M_X64" x86_64)

    detect_architecture("__i386" x86)
    detect_architecture("__i386__" x86)
    detect_architecture("_M_IX86" x86)

    detect_architecture("__ia64" ia64)
    detect_architecture("__ia64__" ia64)
    detect_architecture("_M_IA64" ia64)

    detect_architecture("__mips" mips)
    detect_architecture("__mips__" mips)
    detect_architecture("_M_MRX000" mips)

    detect_architecture("__powerpc64__" ppc64)
    detect_architecture("__ppc64__" ppc64)
    detect_architecture("__PPC64__" ppc64)
    detect_architecture("_ARCH_PPC64" ppc64)

    detect_architecture("__ppc__" ppc)
    detect_architecture("__ppc" ppc)
    detect_architecture("__powerpc__" ppc)
    detect_architecture("_ARCH_COM" ppc)
    detect_architecture("_ARCH_PWR" ppc)
    detect_architecture("_ARCH_PPC" ppc)
    detect_architecture("_M_MPPC" ppc)
    detect_architecture("_M_PPC" ppc)

    detect_architecture("__riscv" riscv)

    detect_architecture("__EMSCRIPTEN__" wasm)
endif()

if (NOT DEFINED ARCHITECTURE)
    set(ARCHITECTURE "GENERIC")
    set(ARCHITECTURE_GENERIC 1)
    add_definitions(-DARCHITECTURE_GENERIC=1)
endif()

message(STATUS "Target architecture: ${ARCHITECTURE}")

if (MSVC AND ARCHITECTURE_x86)
    message(FATAL_ERROR "Attempting to build with the x86 environment is not supported. \
        This can typically happen if you used the Developer Command Prompt from the start menu; \
        instead, run vcvars64.bat directly, located at C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat")
endif()

if (CXX_CLANG_CL)
    add_compile_options(
        # clang-cl prints literally 10000+ warnings without this
        $<$<COMPILE_LANGUAGE:C,CXX>:-Wno-unused-command-line-argument>
        $<$<COMPILE_LANGUAGE:C,CXX>:-Wno-unsafe-buffer-usage>
        $<$<COMPILE_LANGUAGE:C,CXX>:-Wno-unused-value>
        $<$<COMPILE_LANGUAGE:C,CXX>:-Wno-extra-semi-stmt>
        $<$<COMPILE_LANGUAGE:C,CXX>:-Wno-sign-conversion>
        $<$<COMPILE_LANGUAGE:C,CXX>:-Wno-reserved-identifier>
        $<$<COMPILE_LANGUAGE:C,CXX>:-Wno-deprecated-declarations>
        $<$<COMPILE_LANGUAGE:C,CXX>:-Wno-cast-function-type-mismatch>
        $<$<COMPILE_LANGUAGE:C,CXX>:/EHsc> # thanks microsoft
    )

    # REQUIRED CPU features IN Windows-amd64
    if (ARCHITECTURE_x86_64)
        add_compile_options(
            $<$<COMPILE_LANGUAGE:C,CXX>:-msse4.1>
            $<$<COMPILE_LANGUAGE:C,CXX>:-mcx16>
        )
    endif()
endif()

set(CPM_SOURCE_CACHE ${CMAKE_SOURCE_DIR}/.cache/cpm)

include(DownloadExternals)
include(CMakeDependentOption)
include(CTest)

# Disable Warnings as Errors for MSVC
if (MSVC AND NOT CXX_CLANG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3 /WX-")
endif()

if (PLATFORM_FREEBSD OR PLATFORM_DRAGONFLYBSD)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L${CMAKE_SYSROOT}/usr/local/lib")
endif()

# Set bundled sdl2/qt as dependent options.
# On Linux system SDL2 is likely to be lacking HIDAPI support which have drawbacks but is needed for SDL motion
cmake_dependent_option(ENABLE_SDL2 "Enable the SDL2 frontend" ON "NOT ANDROID" OFF)

# TODO(crueter): Cleanup, each dep that has a bundled option should allow to choose between bundled, external, system
cmake_dependent_option(YUZU_USE_EXTERNAL_SDL2 "Build SDL2 from external source" OFF "ENABLE_SDL2;NOT MSVC" OFF)
cmake_dependent_option(YUZU_USE_BUNDLED_SDL2 "Download bundled SDL2 build" "${MSVC}" "ENABLE_SDL2" OFF)

# qt stuff
option(ENABLE_QT "Enable the Qt frontend" ON)
option(ENABLE_QT_TRANSLATION "Enable translations for the Qt frontend" OFF)
option(ENABLE_UPDATE_CHECKER "Enable update checker (for Qt and Android)" OFF)
cmake_dependent_option(YUZU_USE_BUNDLED_QT "Download bundled Qt binaries" "${MSVC}" "ENABLE_QT" OFF)
option(YUZU_USE_QT_MULTIMEDIA "Use QtMultimedia for Camera" OFF)
option(YUZU_USE_QT_WEB_ENGINE "Use QtWebEngine for web applet implementation" OFF)
set(YUZU_QT_MIRROR "" CACHE STRING "What mirror to use for downloading the bundled Qt libraries")

option(ENABLE_CUBEB "Enables the cubeb audio backend" ON)

set(EXT_DEFAULT OFF)
if (MSVC OR ANDROID)
    set(EXT_DEFAULT ON)
endif()
option(YUZU_USE_CPM "Use CPM to fetch system dependencies (fmt, boost, etc) if needed. Externals will still be fetched." ${EXT_DEFAULT})

# ffmpeg
option(YUZU_USE_BUNDLED_FFMPEG "Download bundled FFmpeg" ${EXT_DEFAULT})
cmake_dependent_option(YUZU_USE_EXTERNAL_FFMPEG "Build FFmpeg from external source" "${PLATFORM_SUN}" "NOT WIN32 AND NOT ANDROID" OFF)

# sirit
set(BUNDLED_SIRIT_DEFAULT OFF)
if ((MSVC AND NOT (CMAKE_BUILD_TYPE MATCHES "Debug|RelWithDebInfo") OR ANDROID))
    set(BUNDLED_SIRIT_DEFAULT ON)
endif()
option(YUZU_USE_BUNDLED_SIRIT "Download bundled sirit" ${BUNDLED_SIRIT_DEFAULT})

# Re-allow on FreeBSD once its on mainline ports
cmake_dependent_option(ENABLE_LIBUSB "Enable the use of LibUSB" ON "WIN32 OR PLATFORM_LINUX OR APPLE" OFF)

cmake_dependent_option(ENABLE_OPENGL "Enable OpenGL" ON "NOT WIN32 OR NOT ARCHITECTURE_arm64" OFF)
mark_as_advanced(FORCE ENABLE_OPENGL)

option(ENABLE_WEB_SERVICE "Enable web services (telemetry, etc.)" ON)
option(ENABLE_WIFI_SCAN "Enable WiFi scanning" OFF)

cmake_dependent_option(USE_DISCORD_PRESENCE "Enables Discord Rich Presence" OFF "ENABLE_QT" OFF)

option(YUZU_TESTS "Compile tests" "${BUILD_TESTING}")

option(YUZU_ENABLE_LTO "Enable link-time optimization" OFF)
if(YUZU_ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT COMPILER_SUPPORTS_LTO)
    if(NOT COMPILER_SUPPORTS_LTO)
        message(FATAL_ERROR "Your compiler does not support interprocedural optimization (IPO). Re-run CMake with -DYUZU_ENABLE_LTO=OFF.")
    endif()
    set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ${COMPILER_SUPPORTS_LTO})
endif()

option(USE_CCACHE "Use ccache for compilation" OFF)
set(CCACHE_PATH "ccache" CACHE STRING "Path to ccache binary")
if(USE_CCACHE)
    find_program(CCACHE_BINARY ${CCACHE_PATH})
    if(CCACHE_BINARY)
        message(STATUS "Found ccache at: ${CCACHE_BINARY}")
        set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_BINARY})
        set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_BINARY})
    else()
        message(FATAL_ERROR "USE_CCACHE enabled, but no executable found at: ${CCACHE_PATH}")
    endif()
    # Follow SCCache recommendations:
    # <https://github.com/mozilla/sccache/blob/main/README.md?plain=1#L144>
    if(WIN32)
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
            string(REPLACE "/Zi" "/Z7" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
        elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
            string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
            string(REPLACE "/Zi" "/Z7" CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
        endif()
    endif()
endif()

# TODO(crueter): CI this?
option(YUZU_DOWNLOAD_ANDROID_VVL "Download validation layer binary for android" ON)

option(YUZU_LEGACY "Apply patches that improve compatibility with older GPUs (e.g. Snapdragon 865) at the cost of performance" OFF)

cmake_dependent_option(YUZU_ROOM "Enable dedicated room functionality" ON "NOT ANDROID" OFF)
cmake_dependent_option(YUZU_ROOM_STANDALONE "Enable standalone room executable" ON "YUZU_ROOM" OFF)

cmake_dependent_option(YUZU_CMD "Compile the eden-cli executable" ON "ENABLE_SDL2;NOT ANDROID" OFF)

cmake_dependent_option(YUZU_CRASH_DUMPS "Compile crash dump (Minidump) support" OFF "WIN32 OR LINUX" OFF)

option(YUZU_DOWNLOAD_TIME_ZONE_DATA "Always download time zone binaries" ON)
set(YUZU_TZDB_PATH "" CACHE STRING "Path to a pre-downloaded timezone database")

cmake_dependent_option(YUZU_USE_FASTER_LD "Check if a faster linker is available" ON "LINUX" OFF)

cmake_dependent_option(YUZU_USE_BUNDLED_MOLTENVK "Download bundled MoltenVK lib" ON "APPLE" OFF)

option(YUZU_DISABLE_LLVM "Disable LLVM (useful for CI)" OFF)

set(DEFAULT_ENABLE_OPENSSL ON)
if (ANDROID OR WIN32 OR APPLE OR PLATFORM_SUN)
    # - Windows defaults to the Schannel backend.
    # - macOS defaults to the SecureTransport backend.
    # - Android currently has no SSL backend as the NDK doesn't include any SSL
    #   library; a proper 'native' backend would have to go through Java.
    # But you can force builds for those platforms to use OpenSSL if you have
    # your own copy of it.
    set(DEFAULT_ENABLE_OPENSSL OFF)
endif()

if (ENABLE_WEB_SERVICE OR USE_DISCORD_PRESENCE)
    set(DEFAULT_ENABLE_OPENSSL ON)
endif()

option(ENABLE_OPENSSL "Enable OpenSSL backend for ISslConnection" ${DEFAULT_ENABLE_OPENSSL})
if (ENABLE_OPENSSL)
    set(DEFAULT_YUZU_USE_BUNDLED_OPENSSL OFF)
    if (EXT_DEFAULT OR PLATFORM_SUN)
        set(DEFAULT_YUZU_USE_BUNDLED_OPENSSL ON)
    endif()
    option(YUZU_USE_BUNDLED_OPENSSL "Download bundled OpenSSL build" ${DEFAULT_YUZU_USE_BUNDLED_OPENSSL})
endif()

if (ANDROID AND YUZU_DOWNLOAD_ANDROID_VVL)
    # TODO(crueter): CPM this
    set(vvl_version "1.4.321.0")
    set(vvl_zip_file "${CMAKE_BINARY_DIR}/externals/vvl-android.zip")
    if (NOT EXISTS "${vvl_zip_file}")
        # Download and extract validation layer release to externals directory
        set(vvl_base_url "https://github.com/KhronosGroup/Vulkan-ValidationLayers/releases/download")
        file(DOWNLOAD "${vvl_base_url}/vulkan-sdk-${vvl_version}/android-binaries-${vvl_version}.zip"
            "${vvl_zip_file}" SHOW_PROGRESS)
        execute_process(COMMAND ${CMAKE_COMMAND} -E tar xf "${vvl_zip_file}"
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/externals")
    endif()

    # Copy the arm64 binary to src/android/app/main/jniLibs
    set(vvl_lib_path "${CMAKE_CURRENT_SOURCE_DIR}/src/android/app/src/main/jniLibs/arm64-v8a/")
    file(COPY "${CMAKE_BINARY_DIR}/externals/android-binaries-${vvl_version}/arm64-v8a/libVkLayer_khronos_validation.so"
        DESTINATION "${vvl_lib_path}")
endif()

if (ANDROID)
    set(CMAKE_SKIP_INSTALL_RULES ON)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5) # Workaround for Oboe
endif()

# Default to a Release build
get_property(IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if (NOT IS_MULTI_CONFIG AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
    message(STATUS "Defaulting to a Release build")
endif()

if(EXISTS ${PROJECT_SOURCE_DIR}/hooks/pre-commit AND NOT EXISTS ${PROJECT_SOURCE_DIR}/.git/hooks/pre-commit)
    if (EXISTS ${PROJECT_SOURCE_DIR}/.git/)
        message(STATUS "Copying pre-commit hook")
        file(COPY hooks/pre-commit DESTINATION ${PROJECT_SOURCE_DIR}/.git/hooks)
    endif()
endif()

configure_file(${PROJECT_SOURCE_DIR}/dist/compatibility_list/compatibility_list.qrc
    ${PROJECT_BINARY_DIR}/dist/compatibility_list/compatibility_list.qrc
    COPYONLY)

if (EXISTS ${PROJECT_SOURCE_DIR}/dist/compatibility_list/compatibility_list.json)
    configure_file("${PROJECT_SOURCE_DIR}/dist/compatibility_list/compatibility_list.json"
        "${PROJECT_BINARY_DIR}/dist/compatibility_list/compatibility_list.json"
        COPYONLY)
endif()

if (ENABLE_COMPATIBILITY_LIST_DOWNLOAD AND NOT EXISTS ${PROJECT_BINARY_DIR}/dist/compatibility_list/compatibility_list.json)
    message(STATUS "Downloading compatibility list for yuzu...")
    file(DOWNLOAD
        https://api.yuzu-emu.org/gamedb/
        "${PROJECT_BINARY_DIR}/dist/compatibility_list/compatibility_list.json" SHOW_PROGRESS)
endif()

if (NOT EXISTS ${PROJECT_BINARY_DIR}/dist/compatibility_list/compatibility_list.json)
    file(WRITE ${PROJECT_BINARY_DIR}/dist/compatibility_list/compatibility_list.json "")
endif()

if (YUZU_LEGACY)
    message(WARNING "Making legacy build. Performance may suffer.")
    add_compile_definitions(YUZU_LEGACY)
endif()

if (ARCHITECTURE_arm64 AND (ANDROID OR PLATFORM_LINUX))
    set(HAS_NCE 1)
    add_compile_definitions(HAS_NCE=1)
endif()

if (YUZU_ROOM)
    add_compile_definitions(YUZU_ROOM)
endif()

if ((ANDROID OR APPLE OR UNIX) AND (NOT PLATFORM_LINUX OR ANDROID) AND NOT WIN32)
    if(CXX_APPLE OR CXX_CLANG)
        # libc++ has stop_token and jthread as experimental
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexperimental-library")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fexperimental-library")
    else()
        # Uses glibc, mostly?
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_LIBCPP_ENABLE_EXPERIMENTAL=1")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_LIBCPP_ENABLE_EXPERIMENTAL=1")
    endif()
endif()

# Build/optimization presets
if (PLATFORM_LINUX OR CXX_CLANG)
    if (ARCHITECTURE_x86_64)
        # See https://gcc.gnu.org/onlinedocs/gcc/x86-Options.html
        # Generic supports Pentium Pro instruction set and above
        set(YUZU_BUILD_PRESET "custom" CACHE STRING "Build preset to use. One of: custom, generic, v3, zen2, zen4, native")
        if (${YUZU_BUILD_PRESET} STREQUAL "generic")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=x86-64 -mtune=generic")
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=x86-64 -mtune=generic")
        elseif (${YUZU_BUILD_PRESET} STREQUAL "v3")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=x86-64-v3 -mtune=generic")
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=x86-64-v3 -mtune=generic")
        elseif (${YUZU_BUILD_PRESET} STREQUAL "zen2")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=znver2 -mtune=znver2")
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=znver2 -mtune=znver2")
        elseif (${YUZU_BUILD_PRESET} STREQUAL "zen4")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=znver4 -mtune=znver4")
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=znver4 -mtune=znver4")
        elseif (${YUZU_BUILD_PRESET} STREQUAL "native")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -mtune=native")
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native -mtune=native")
        endif()
    elseif(ARCHITECTURE_arm64)
        # See https://gcc.gnu.org/onlinedocs/gcc/AArch64-Options.html
        set(YUZU_BUILD_PRESET "custom" CACHE STRING "Build preset to use. One of: custom, generic, armv9, native")
        if (${YUZU_BUILD_PRESET} STREQUAL "generic")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv8-a -mtune=generic")
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv8-a -mtune=generic")
        elseif (${YUZU_BUILD_PRESET} STREQUAL "armv9")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv9-a -mtune=generic")
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv9-a -mtune=generic")
        elseif (${YUZU_BUILD_PRESET} STREQUAL "native")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -mtune=native")
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native -mtune=native")
        endif()
    endif()
endif()

# Other presets, e.g. steamdeck
set(YUZU_SYSTEM_PROFILE "generic" CACHE STRING "CMake and Externals profile to use. One of: generic, steamdeck")

# Configure C++ standard
# ===========================

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output binaries to bin/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

# System imported libraries
# =======================================================================

include(CPMUtil)

# openssl funniness
if (ENABLE_OPENSSL)
    if (YUZU_USE_BUNDLED_OPENSSL)
        AddJsonPackage(openssl)
        if (OpenSSL_ADDED)
            add_compile_definitions(YUZU_BUNDLED_OPENSSL)
        endif()
    endif()

    find_package(OpenSSL 1.1.1 REQUIRED)
endif()

if (YUZU_USE_CPM)
    message(STATUS "Fetching needed dependencies with CPM")

    set(BUILD_SHARED_LIBS OFF)
    set(BUILD_TESTING OFF)
    set(ENABLE_TESTING OFF)

    # TODO(crueter): renderdoc?

    # boost
    set(BOOST_INCLUDE_LIBRARIES algorithm icl pool container heap asio headers process filesystem crc variant)

    AddJsonPackage(boost)

    # really annoying thing where boost::headers doesn't work with cpm
    # TODO(crueter) investigate
    set(BOOST_NO_HEADERS ${Boost_ADDED})

    if (Boost_ADDED)
        if (MSVC OR ANDROID)
            add_compile_definitions(YUZU_BOOST_v1)
        endif()

        if (NOT MSVC OR CXX_CLANG)
            # boost sucks
            if (PLATFORM_SUN)
                set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthreads")
                set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthreads")
            endif()

            target_compile_options(boost_heap INTERFACE -Wno-shadow)
            target_compile_options(boost_icl INTERFACE -Wno-shadow)
            target_compile_options(boost_asio INTERFACE -Wno-conversion -Wno-implicit-fallthrough)
        endif()
    endif()

    # fmt
    AddJsonPackage(fmt)

    # lz4
    AddJsonPackage(lz4)

    if (lz4_ADDED)
        add_library(lz4::lz4 ALIAS lz4_static)
    endif()

    # nlohmann
    AddJsonPackage(nlohmann)

    # zlib
    AddJsonPackage(zlib)

    if (ZLIB_ADDED)
        add_library(ZLIB::ZLIB ALIAS zlibstatic)
    endif()

    # zstd
    AddJsonPackage(zstd)

    if (zstd_ADDED)
        add_library(zstd::zstd ALIAS libzstd_static)
        add_library(zstd::libzstd ALIAS libzstd_static)
    endif()

    # Opus
    AddJsonPackage(opus)

    if (Opus_ADDED)
        if (MSVC AND CXX_CLANG)
            target_compile_options(opus PRIVATE
                -Wno-implicit-function-declaration
            )
        endif()
    endif()

    if (NOT TARGET Opus::opus)
        add_library(Opus::opus ALIAS opus)
    endif()
else()
    # Enforce the search mode of non-required packages for better and shorter failure messages
    find_package(fmt 8 REQUIRED)

    if (NOT YUZU_DISABLE_LLVM)
        find_package(LLVM MODULE COMPONENTS Demangle)
    endif()

    find_package(nlohmann_json 3.8 REQUIRED)
    find_package(lz4 REQUIRED)
    find_package(RenderDoc MODULE)
    find_package(stb MODULE)

    find_package(Opus 1.3 MODULE REQUIRED)

    find_package(ZLIB 1.2 REQUIRED)
    find_package(zstd 1.5 REQUIRED MODULE)

    # wow
    find_package(Boost 1.57.0 CONFIG REQUIRED OPTIONAL_COMPONENTS headers context system fiber filesystem)

    if (CMAKE_SYSTEM_NAME STREQUAL "Linux" OR ANDROID)
        find_package(gamemode 1.7 MODULE)
    endif()

    if (ENABLE_OPENSSL)
        find_package(OpenSSL 1.1.1 REQUIRED)
    endif()
endif()

if(NOT TARGET Boost::headers)
    AddJsonPackage(boost_headers)
endif()

# List of all FFmpeg components required
set(FFmpeg_COMPONENTS
    avcodec
    avfilter
    avutil
    swscale)

# This function should be passed a list of all files in a target. It will automatically generate
# file groups following the directory hierarchy, so that the layout of the files in IDEs matches the
# one in the filesystem.
function(create_target_directory_groups target_name)
    # Place any files that aren't in the source list in a separate group so that they don't get in
    # the way.
    source_group("Other Files" REGULAR_EXPRESSION ".")

    get_target_property(target_sources "${target_name}" SOURCES)

    foreach(file_name IN LISTS target_sources)
        get_filename_component(dir_name "${file_name}" PATH)
        # Group names use '\' as a separator even though the entire rest of CMake uses '/'...
        string(REPLACE "/" "\\" group_name "${dir_name}")
        source_group("${group_name}" FILES "${file_name}")
    endforeach()
endfunction()

# Platform-specific library requirements
# Put these BEFORE EXTERNALS or Boost WILL die
# =============================================

if (APPLE)
    # Umbrella framework for everything GUI-related
    find_library(COCOA_LIBRARY Cocoa)
    set(PLATFORM_LIBRARIES ${COCOA_LIBRARY} ${IOKIT_LIBRARY} ${COREVIDEO_LIBRARY})
    # find_library(ICONV_LIBRARY iconv REQUIRED)
    # list(APPEND PLATFORM_LIBRARIES ${ICONV_LIBRARY})
elseif (WIN32)
    # Target Windows 10
    add_compile_definitions(_WIN32_WINNT=0x0A00 WINVER=0x0A00)
    set(PLATFORM_LIBRARIES ${PLATFORM_LIBRARIES} winmm iphlpapi ws2_32 wlanapi)
    if (MINGW)
        # PSAPI is the Process Status API
        set(PLATFORM_LIBRARIES ${PLATFORM_LIBRARIES} psapi imm32 version crypt32 rpcrt4 gdi32 wldap32 mswsock)
    endif()
elseif (PLATFORM_HAIKU)
    # Haiku is so special :)
    # Some fucking genius decided to name an entire module "network" in 2019
    # this caused great disaster amongst the Haiku community who had came first with
    # their "libnetwork.so"; since CMake doesn't do magic, we have to use an ABSOLUTE PATH
    # to the library itself, otherwise it will think we are linking to... our network thing
    set(PLATFORM_LIBRARIES bsd /boot/system/lib/libnetwork.so)
elseif (CMAKE_SYSTEM_NAME MATCHES "^(Linux|kFreeBSD|GNU|SunOS)$")
    set(PLATFORM_LIBRARIES rt)
endif()

message(STATUS "Platform Libraries: ${PLATFORM_LIBRARIES}")

add_subdirectory(externals)

# pass targets from externals
find_package(libusb)
find_package(VulkanMemoryAllocator)
find_package(enet)
find_package(MbedTLS)
find_package(VulkanUtilityLibraries)
find_package(SimpleIni)
find_package(SPIRV-Tools)
find_package(sirit)

if (ARCHITECTURE_x86 OR ARCHITECTURE_x86_64)
    find_package(xbyak)
endif()

if (ENABLE_WEB_SERVICE OR ENABLE_QT_UPDATE_CHECKER)
    # Workaround: httplib will kill itself if you attempt to do a find_package propagation
    # find_package(httplib CONFIG)
endif()

if (ENABLE_WEB_SERVICE OR ENABLE_UPDATE_CHECKER)
    find_package(cpp-jwt)
endif()

if (ARCHITECTURE_arm64 OR DYNARMIC_TESTS)
    find_package(oaknut)
endif()

if (ENABLE_SDL2)
    find_package(SDL2)
endif()

if (USE_DISCORD_PRESENCE)
    find_package(DiscordRPC)
endif()

if (ENABLE_CUBEB)
    find_package(cubeb)
endif()

if (YUZU_TESTS OR DYNARMIC_TESTS)
    find_package(Catch2)
endif()

if (ENABLE_QT)
    if (YUZU_USE_BUNDLED_QT)
        download_qt(6.8.3)
    else()
        message(STATUS "Using system Qt")
        if (NOT Qt6_DIR)
            set(Qt6_DIR "" CACHE PATH "Additional path to search for Qt6 libraries like C:/Qt/6.8.3/msvc2022_64/lib/cmake/Qt6")
        endif()
        list(APPEND CMAKE_PREFIX_PATH "${Qt6_DIR}")
    endif()

    find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent)

    if (YUZU_USE_QT_MULTIMEDIA)
        find_package(Qt6 REQUIRED COMPONENTS Multimedia)
    endif()

    if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        # yes Qt, we get it
        set(QT_NO_PRIVATE_MODULE_WARNING ON)
        find_package(Qt6 REQUIRED COMPONENTS DBus OPTIONAL_COMPONENTS GuiPrivate)
    elseif (UNIX AND NOT APPLE)
        find_package(Qt6 REQUIRED COMPONENTS DBus Gui)
    endif()

    if (ENABLE_QT_TRANSLATION)
        find_package(Qt6 REQUIRED COMPONENTS LinguistTools)
    endif()

    if (NOT DEFINED QT_TARGET_PATH)
        get_target_property(qtcore_path Qt6::Core LOCATION_Release)
        string(FIND "${qtcore_path}" "/bin/" qtcore_path_bin_pos REVERSE)
        string(FIND "${qtcore_path}" "/lib/" qtcore_path_lib_pos REVERSE)
        if (qtcore_path_bin_pos GREATER qtcore_path_lib_pos)
            string(SUBSTRING "${qtcore_path}" 0 ${qtcore_path_bin_pos} QT_TARGET_PATH)
        else()
            string(SUBSTRING "${qtcore_path}" 0 ${qtcore_path_lib_pos} QT_TARGET_PATH)
        endif()
    endif()

    if (NOT DEFINED QT_HOST_PATH)
        set(QT_HOST_PATH "${QT_TARGET_PATH}")
    endif()

    message(STATUS "Using target Qt at ${QT_TARGET_PATH}")
    message(STATUS "Using host Qt at ${QT_HOST_PATH}")
endif()

function(set_yuzu_qt_components)
    # Best practice is to ask for all components at once, so they are from the same version
    set(YUZU_QT_COMPONENTS2 Core Widgets Concurrent)
    if (PLATFORM_LINUX)
        list(APPEND YUZU_QT_COMPONENTS2 DBus)
    endif()
    if (YUZU_USE_QT_MULTIMEDIA)
        list(APPEND YUZU_QT_COMPONENTS2 Multimedia)
    endif()
    if (YUZU_USE_QT_WEB_ENGINE)
        list(APPEND YUZU_QT_COMPONENTS2 WebEngineCore WebEngineWidgets)
    endif()
    if (ENABLE_QT_TRANSLATION)
        list(APPEND YUZU_QT_COMPONENTS2 LinguistTools)
    endif()
    set(YUZU_QT_COMPONENTS ${YUZU_QT_COMPONENTS2} PARENT_SCOPE)
endfunction(set_yuzu_qt_components)

if(ENABLE_QT)
    set_yuzu_qt_components()
    find_package(Qt6 REQUIRED COMPONENTS ${YUZU_QT_COMPONENTS})
    set(QT_MAJOR_VERSION 6)
    # Qt6 sets cxx_std_17 and we need to undo that
    set_target_properties(Qt6::Platform PROPERTIES INTERFACE_COMPILE_FEATURES "")
endif()

if (UNIX AND NOT APPLE AND NOT ANDROID)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBVA libva)
endif()

if (NOT (YUZU_USE_BUNDLED_FFMPEG OR YUZU_USE_EXTERNAL_FFMPEG))
    # Use system installed FFmpeg
    find_package(FFmpeg REQUIRED QUIET COMPONENTS ${FFmpeg_COMPONENTS})

    # TODO(crueter): Version
    set_property(GLOBAL APPEND PROPERTY CPM_PACKAGE_NAMES FFmpeg)
    set_property(GLOBAL APPEND PROPERTY CPM_PACKAGE_SHAS "unknown (system)")
    set_property(GLOBAL APPEND PROPERTY CPM_PACKAGE_URLS "https://github.com/FFmpeg/FFmpeg")
endif()

if (WIN32 AND YUZU_CRASH_DUMPS)
    set(BREAKPAD_VER "breakpad-c89f9dd")
    download_bundled_external("breakpad/" ${BREAKPAD_VER} "breakpad-win" BREAKPAD_PREFIX "c89f9dd")

    set(BREAKPAD_CLIENT_INCLUDE_DIR "${BREAKPAD_PREFIX}/include")
    set(BREAKPAD_CLIENT_LIBRARY "${BREAKPAD_PREFIX}/lib/libbreakpad_client.lib")

    add_library(libbreakpad_client INTERFACE IMPORTED)
    target_link_libraries(libbreakpad_client INTERFACE "${BREAKPAD_CLIENT_LIBRARY}")
    target_include_directories(libbreakpad_client INTERFACE "${BREAKPAD_CLIENT_INCLUDE_DIR}")
endif()

# Prefer the -pthread flag on Linux.
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Setup a custom clang-format target (if clang-format can be found) that will run
# against all the src files. This should be used before making a pull request.
# =======================================================================

set(CLANG_FORMAT_POSTFIX "-15")
find_program(CLANG_FORMAT
    NAMES clang-format${CLANG_FORMAT_POSTFIX}
    clang-format
    PATHS ${PROJECT_BINARY_DIR}/externals)
# if find_program doesn't find it, try to download from externals
if (NOT CLANG_FORMAT)
    if (WIN32 AND NOT CMAKE_CROSSCOMPILING)
        message(STATUS "Clang format not found! Downloading...")
        set(CLANG_FORMAT "${PROJECT_BINARY_DIR}/externals/clang-format${CLANG_FORMAT_POSTFIX}.exe")
        file(DOWNLOAD
            https://github.com/eden-emulator/ext-windows-bin/raw/master/clang-format${CLANG_FORMAT_POSTFIX}.exe
            "${CLANG_FORMAT}" SHOW_PROGRESS
            STATUS DOWNLOAD_SUCCESS)
        if (NOT DOWNLOAD_SUCCESS EQUAL 0)
            message(WARNING "Could not download clang format! Disabling the clang format target")
            file(REMOVE ${CLANG_FORMAT})
            unset(CLANG_FORMAT)
        endif()
    else()
        message(WARNING "Clang format not found! Disabling the clang format target")
    endif()
endif()

if (CLANG_FORMAT)
    set(SRCS ${PROJECT_SOURCE_DIR}/src)
    set(CCOMMENT "Running clang format against all the .h and .cpp files in src/")
    if (WIN32)
        add_custom_target(clang-format
            COMMAND powershell.exe -Command "Get-ChildItem '${SRCS}/*' -Include *.cpp,*.h -Recurse | Foreach {&'${CLANG_FORMAT}' -i $_.fullname}"
            COMMENT ${CCOMMENT})
    elseif(MINGW)
        add_custom_target(clang-format
            COMMAND find `cygpath -u ${SRCS}` -iname *.h -o -iname *.cpp | xargs `cygpath -u ${CLANG_FORMAT}` -i
            COMMENT ${CCOMMENT})
    else()
        add_custom_target(clang-format
            COMMAND find ${SRCS} -iname *.h -o -iname *.cpp | xargs ${CLANG_FORMAT} -i
            COMMENT ${CCOMMENT})
    endif()
    unset(SRCS)
    unset(CCOMMENT)
endif()

# Include source code
# ===================

# Adjustments for MSVC + Ninja
if (MSVC AND CMAKE_GENERATOR STREQUAL "Ninja")
    add_compile_options(
        /wd4464 # relative include path contains '..'
        /wd4711 # function 'function' selected for automatic inline expansion
        /wd4820 # 'bytes' bytes padding added after construct 'member_name'
    )
endif()

# Adjustments for clang-cl
if (MSVC AND CXX_CLANG)
    if (ARCHITECTURE_x86_64)
        set(FILE_ARCH x86_64)
    elseif (ARCHITECTURE_arm64)
        set(FILE_ARCH aarch64)
    else()
        message(FATAL_ERROR "clang-cl: Unsupported architecture ${ARCHITECTURE}")
    endif()

    AddJsonPackage(llvm-mingw)
    set(LIB_PATH "${llvm-mingw_SOURCE_DIR}/libclang_rt.builtins-${FILE_ARCH}.a")

    add_library(llvm-mingw-runtime STATIC IMPORTED)
    set_target_properties(llvm-mingw-runtime PROPERTIES
        IMPORTED_LOCATION "${LIB_PATH}"
    )

    link_libraries(llvm-mingw-runtime)
endif()

#[[
    search order:
    - gold (GCC only) - the best, generally, but unfortunately not packaged anymore
    - mold (GCC only) - generally does well on GCC
    - ldd - preferred on clang
    - bfd - the final fallback
    - If none are found (macOS uses ld.prime, etc) just use the default linker
]]
if (YUZU_USE_FASTER_LD)
    find_program(LINKER_BFD bfd)
    if (LINKER_BFD)
        set(LINKER bfd)
    endif()

    find_program(LINKER_LLD lld)
    if (LINKER_LLD)
        set(LINKER lld)
    endif()

    if (CXX_GCC)
        find_program(LINKER_MOLD mold)
        if (LINKER_MOLD AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "12.1")
            set(LINKER mold)
        endif()

        find_program(LINKER_GOLD gold)
        if (LINKER_GOLD)
            set(LINKER gold)
        endif()
    endif()

    if (LINKER)
        message(NOTICE "Selecting ${LINKER} as linker")
        add_link_options("-fuse-ld=${LINKER}")
    else()
        message(WARNING "No faster linker found--using default")
    endif()

    if (LINKER STREQUAL "lld" AND CXX_GCC)
        message(WARNING "Using lld on GCC may cause issues with certain LTO settings. If the program fails to compile, disable YUZU_USE_FASTER_LD, or install mold or GNU gold.")
    endif()
endif()

# Set runtime library to MD/MDd for all configurations
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

    # Force all projects (including external dependencies) to use the same runtime
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MDd")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MD")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MDd")

    # Add this to ensure Cubeb uses the same runtime
    add_compile_options(
        $<$<CONFIG:Debug>:/MDd>
        $<$<CONFIG:Release>:/MD>
        $<$<CONFIG:RelWithDebInfo>:/MD>
        $<$<CONFIG:MinSizeRel>:/MD>
    )
endif()

if (MINGW)
    # This saves a truly ridiculous amount of time during linking
    # In my tests, without this it takes 2 mins, with it takes 3-5 seconds
    # or on GitHub Actions, 10 minutes -> 3 seconds
    set(MINGW_FLAGS "-Wl,--strip-all -Wl,--gc-sections")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} ${MINGW_FLAGS}")
endif()

add_subdirectory(src)

# Set yuzu project or yuzu-cmd project as default StartUp Project in Visual Studio depending on whether QT is enabled or not
if(ENABLE_QT)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT yuzu)
else()
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT yuzu-cmd)
endif()

# Installation instructions
# =========================

# Install freedesktop.org metadata files, following those specifications:
# https://specifications.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html
# https://specifications.freedesktop.org/icon-theme-spec/icon-theme-spec-latest.html
# https://specifications.freedesktop.org/shared-mime-info-spec/shared-mime-info-spec-latest.html
# https://www.freedesktop.org/software/appstream/docs/
if(ENABLE_QT AND UNIX AND NOT APPLE)
    install(FILES "dist/dev.eden_emu.eden.desktop"
        DESTINATION "share/applications")
    install(FILES "dist/dev.eden_emu.eden.svg"
        DESTINATION "share/icons/hicolor/scalable/apps")

    # TODO: these files need to be updated.
    install(FILES "dist/dev.eden_emu.eden.xml"
        DESTINATION "share/mime/packages")
    install(FILES "dist/dev.eden_emu.eden.metainfo.xml"
        DESTINATION "share/metainfo")
endif()
