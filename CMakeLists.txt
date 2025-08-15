# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.22)

project(yuzu)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/externals/cmake-modules")
if (${CMAKE_SYSTEM_NAME} STREQUAL "SunOS")
    # Terrific Solaris pkg shenanigans
    list(APPEND CMAKE_PREFIX_PATH "/usr/lib/qt/6.6/lib/amd64/cmake")
    list(APPEND CMAKE_MODULE_PATH "/usr/lib/qt/6.6/lib/amd64/cmake")
endif()

set(CPM_SOURCE_CACHE ${CMAKE_SOURCE_DIR}/.cache/cpm)

include(DownloadExternals)
include(CMakeDependentOption)
include(CTest)

# Disable Warnings as Errors for MSVC
if (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3 /WX-")
endif()

if (${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L/usr/local/lib")
endif()

# Check if SDL2::SDL2 target exists; if not, create an alias
if (TARGET SDL2::SDL2-static)
    add_library(SDL2::SDL2 ALIAS SDL2::SDL2-static)
elseif (TARGET SDL2::SDL2-shared)
    add_library(SDL2::SDL2 ALIAS SDL2::SDL2-shared)
endif()

# Set bundled sdl2/qt as dependent options.
# On Linux system SDL2 is likely to be lacking HIDAPI support which have drawbacks but is needed for SDL motion
option(ENABLE_SDL2 "Enable the SDL2 frontend" ON)
CMAKE_DEPENDENT_OPTION(YUZU_USE_BUNDLED_SDL2 "Download bundled SDL2 binaries" ON "ENABLE_SDL2;MSVC" OFF)

if (${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
    CMAKE_DEPENDENT_OPTION(YUZU_USE_EXTERNAL_SDL2 "Compile external SDL2" OFF "ENABLE_SDL2;NOT MSVC" OFF)
else()
    CMAKE_DEPENDENT_OPTION(YUZU_USE_EXTERNAL_SDL2 "Compile external SDL2" ON "ENABLE_SDL2;NOT MSVC" OFF)
endif()

cmake_dependent_option(ENABLE_LIBUSB "Enable the use of LibUSB" ON "NOT ANDROID" OFF)

option(ENABLE_OPENGL "Enable OpenGL" ON)
mark_as_advanced(FORCE ENABLE_OPENGL)

option(ENABLE_QT "Enable the Qt frontend" ON)
option(ENABLE_QT_TRANSLATION "Enable translations for the Qt frontend" OFF)
option(ENABLE_QT_UPDATE_CHECKER "Enable update checker for the Qt frontend" OFF)

CMAKE_DEPENDENT_OPTION(YUZU_USE_BUNDLED_QT "Download bundled Qt binaries" "${MSVC}" "ENABLE_QT" OFF)

# TODO(crueter): maybe this should default on everywhere...?
if (MSVC OR ANDROID)
    set(CPM_DEFAULT ON)
else()
    set(CPM_DEFAULT OFF)
endif()

option(YUZU_USE_CPM "Use CPM for Eden dependencies" "${CPM_DEFAULT}")

option(ENABLE_WEB_SERVICE "Enable web services (telemetry, etc.)" ON)
option(ENABLE_WIFI_SCAN "Enable WiFi scanning" OFF)

if (${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
    option(YUZU_USE_BUNDLED_FFMPEG "Download/Build bundled FFmpeg" OFF)
else()
    option(YUZU_USE_BUNDLED_FFMPEG "Download/Build bundled FFmpeg" ON)
endif()

if (${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
    option(YUZU_USE_EXTERNAL_VULKAN_HEADERS "Use Vulkan-Headers from externals" OFF)
else()
    option(YUZU_USE_EXTERNAL_VULKAN_HEADERS "Use Vulkan-Headers from externals" ON)
endif()

if (${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
    option(YUZU_USE_EXTERNAL_VULKAN_UTILITY_LIBRARIES "Use Vulkan-Utility-Libraries from externals" OFF)
else()
    option(YUZU_USE_EXTERNAL_VULKAN_UTILITY_LIBRARIES "Use Vulkan-Utility-Libraries from externals" ON)
endif()

if (${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
    option(YUZU_USE_EXTERNAL_VULKAN_SPIRV_TOOLS "Use SPIRV-Tools from externals" OFF)
else()
    option(YUZU_USE_EXTERNAL_VULKAN_SPIRV_TOOLS "Use SPIRV-Tools from externals" ON)
endif()

option(YUZU_USE_QT_MULTIMEDIA "Use QtMultimedia for Camera" OFF)

option(YUZU_USE_QT_WEB_ENGINE "Use QtWebEngine for web applet implementation" OFF)

set(YUZU_QT_MIRROR "" CACHE STRING "What mirror to use for downloading the bundled Qt libraries")

option(ENABLE_CUBEB "Enables the cubeb audio backend" ON)

option(USE_DISCORD_PRESENCE "Enables Discord Rich Presence" OFF)

option(ENABLE_MICROPROFILE "Enables microprofile capabilities" OFF)

option(YUZU_TESTS "Compile tests" "${BUILD_TESTING}")

if (${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
    option(YUZU_USE_PRECOMPILED_HEADERS "Use precompiled headers" OFF)
else()
    option(YUZU_USE_PRECOMPILED_HEADERS "Use precompiled headers" ON)
endif()

option(YUZU_DOWNLOAD_ANDROID_VVL "Download validation layer binary for android" ON)

option(FORCE_DOWNLOAD_WIN_BUNDLES "Forcefully download bundled Windows dependencies (useful for CI)" OFF)

CMAKE_DEPENDENT_OPTION(YUZU_ROOM "Enable dedicated room functionality" ON "NOT ANDROID" OFF)

CMAKE_DEPENDENT_OPTION(YUZU_ROOM_STANDALONE "Enable standalone room executable" ON "YUZU_ROOM" OFF)

CMAKE_DEPENDENT_OPTION(YUZU_CMD "Compile the eden-cli executable" ON "NOT ANDROID" OFF)

CMAKE_DEPENDENT_OPTION(YUZU_CRASH_DUMPS "Compile crash dump (Minidump) support" OFF "WIN32 OR LINUX" OFF)

if (${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
    option(YUZU_CHECK_SUBMODULES "Check if submodules are present" OFF)
else()
    option(YUZU_CHECK_SUBMODULES "Check if submodules are present" ON)
endif()

option(YUZU_ENABLE_LTO "Enable link-time optimization" OFF)

option(YUZU_DOWNLOAD_TIME_ZONE_DATA "Always download time zone binaries" ON)

option(YUZU_ENABLE_PORTABLE "Allow yuzu to enable portable mode if a user folder is found in the CWD" ON)

CMAKE_DEPENDENT_OPTION(YUZU_USE_FASTER_LD "Check if a faster linker is available" ON "NOT WIN32" OFF)

CMAKE_DEPENDENT_OPTION(USE_SYSTEM_MOLTENVK "Use the system MoltenVK lib (instead of the bundled one)" OFF "APPLE" OFF)

set(DEFAULT_ENABLE_OPENSSL ON)
if (ANDROID OR WIN32 OR APPLE OR ${CMAKE_SYSTEM_NAME} STREQUAL "SunOS")
    # - Windows defaults to the Schannel backend.
    # - macOS defaults to the SecureTransport backend.
    # - Android currently has no SSL backend as the NDK doesn't include any SSL
    #   library; a proper 'native' backend would have to go through Java.
    # But you can force builds for those platforms to use OpenSSL if you have
    # your own copy of it.
    set(DEFAULT_ENABLE_OPENSSL OFF)
endif()

if (ENABLE_WEB_SERVICE)
    set(DEFAULT_ENABLE_OPENSSL ON)
endif()

option(ENABLE_OPENSSL "Enable OpenSSL backend for ISslConnection" ${DEFAULT_ENABLE_OPENSSL})

if (ANDROID AND YUZU_DOWNLOAD_ANDROID_VVL)
    set(vvl_version "sdk-1.3.261.1")
    set(vvl_zip_file "${CMAKE_BINARY_DIR}/externals/vvl-android.zip")
    if (NOT EXISTS "${vvl_zip_file}")
        # Download and extract validation layer release to externals directory
        set(vvl_base_url "https://github.com/KhronosGroup/Vulkan-ValidationLayers/releases/download")
        file(DOWNLOAD "${vvl_base_url}/${vvl_version}/android-binaries-${vvl_version}-android.zip"
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

if (YUZU_USE_PRECOMPILED_HEADERS)
    if (MSVC AND CCACHE)
        # buildcache does not properly cache PCH files, leading to compilation errors.
        # See https://github.com/mbitsnbites/buildcache/discussions/230
        message(WARNING "buildcache does not properly support Precompiled Headers. Disabling PCH")
        set(DYNARMIC_USE_PRECOMPILED_HEADERS OFF CACHE BOOL "" FORCE)
        set(YUZU_USE_PRECOMPILED_HEADERS OFF CACHE BOOL "" FORCE)
    endif()
endif()
if (YUZU_USE_PRECOMPILED_HEADERS)
    message(STATUS "Using Precompiled Headers.")
    set(CMAKE_PCH_INSTANTIATE_TEMPLATES ON)
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

# Sanity check : Check that all submodules are present
# =======================================================================

function(check_submodules_present)
    file(READ "${PROJECT_SOURCE_DIR}/.gitmodules" gitmodules)
    string(REGEX MATCHALL "path *= *[^ \t\r\n]*" gitmodules ${gitmodules})
    foreach(module ${gitmodules})
        string(REGEX REPLACE "path *= *" "" module ${module})

        file(GLOB RESULT "${PROJECT_SOURCE_DIR}/${module}/*")
        list(LENGTH RESULT RES_LEN)
        if(RES_LEN EQUAL 0)
            message(FATAL_ERROR "Git submodule ${module} not found. "
                "Please run: \ngit submodule update --init --recursive")
        endif()
        if (EXISTS "${PROJECT_SOURCE_DIR}/${module}/.git")
            set(SUBMODULE_DIR "${PROJECT_SOURCE_DIR}/${module}")

            execute_process(
                COMMAND git rev-parse --short=10 HEAD
                WORKING_DIRECTORY ${SUBMODULE_DIR}
                OUTPUT_VARIABLE SUBMODULE_SHA
            )

            # would probably be better to do string parsing, but whatever
            execute_process(
                COMMAND git remote get-url origin
                WORKING_DIRECTORY ${SUBMODULE_DIR}
                OUTPUT_VARIABLE SUBMODULE_URL
            )

            string(REGEX REPLACE "\n|\r" "" SUBMODULE_SHA ${SUBMODULE_SHA})
            string(REGEX REPLACE "\n|\r|\\.git" "" SUBMODULE_URL ${SUBMODULE_URL})

            get_filename_component(SUBMODULE_NAME ${SUBMODULE_DIR} NAME)

            set_property(GLOBAL APPEND PROPERTY CPM_PACKAGE_NAMES ${SUBMODULE_NAME})
            set_property(GLOBAL APPEND PROPERTY CPM_PACKAGE_SHAS ${SUBMODULE_SHA})
            set_property(GLOBAL APPEND PROPERTY CPM_PACKAGE_URLS ${SUBMODULE_URL})
        endif()
    endforeach()
endfunction()

if(EXISTS ${PROJECT_SOURCE_DIR}/.gitmodules AND YUZU_CHECK_SUBMODULES)
    check_submodules_present()
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
            add_definitions(-DARCHITECTURE_${arch}=1)
        endif()
    endif()
endfunction()

if (NOT ENABLE_GENERIC)
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
endif()

if (NOT DEFINED ARCHITECTURE)
    set(ARCHITECTURE "GENERIC")
    set(ARCHITECTURE_GENERIC 1)
    add_definitions(-DARCHITECTURE_GENERIC=1)
endif()
message(STATUS "Target architecture: ${ARCHITECTURE}")

if (UNIX)
    add_definitions(-DYUZU_UNIX=1)
endif()

if (ARCHITECTURE_arm64 AND (ANDROID OR APPLE OR ${CMAKE_SYSTEM_NAME} STREQUAL "Linux"))
    set(HAS_NCE 1)
    add_definitions(-DHAS_NCE=1)
endif()

if (YUZU_ROOM)
    add_definitions(-DYUZU_ROOM)
endif()

# Build/optimization presets
if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    if (ARCHITECTURE_x86_64)
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
        set(YUZU_BUILD_PRESET "custom" CACHE STRING "Build preset to use. One of: custom, generic, armv9")
        if (${YUZU_BUILD_PRESET} STREQUAL "generic")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv8-a -mtune=generic")
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv8-a -mtune=generic")
        elseif (${YUZU_BUILD_PRESET} STREQUAL "armv9")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv9-a -mtune=generic")
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv9-a -mtune=generic")
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

if (YUZU_USE_CPM)
    include(CPMUtil)
    message(STATUS "Fetching needed dependencies with CPM")

    set(BUILD_SHARED_LIBS OFF)
    set(BUILD_TESTING OFF)

    # TODO(crueter): renderdoc?

    # openssl funniness
    if (ENABLE_OPENSSL)
        if (MSVC)
            set(BUILD_SHARED_LIBS OFF)
            AddPackage(
                NAME OpenSSL
                REPO crueter/OpenSSL-CI
                TAG v3.5.2
                VERSION 3.5.2
                ARTIFACT openssl-windows-3.5.2.tar.zst

                KEY windows
                HASH_SUFFIX sha512sum
                BUNDLED_PACKAGE ON
            )

            include(${OpenSSL_SOURCE_DIR}/openssl.cmake)
        endif()

        if (ANDROID)
            set(BUILD_SHARED_LIBS OFF)
            AddPackage(
                NAME OpenSSL
                REPO crueter/OpenSSL-CI
                TAG v3.5.2
                VERSION 3.5.2
                ARTIFACT openssl-android-3.5.2.tar.zst

                KEY android
                HASH_SUFFIX sha512sum
                BUNDLED_PACKAGE ON
            )

            include(${OpenSSL_SOURCE_DIR}/openssl.cmake)
        endif()
    endif()

    AddPackage(
        NAME Boost
        REPO boostorg/boost
        TAG boost-1.88.0
        ARTIFACT boost-1.88.0-cmake.7z

        HASH e5b049e5b61964480ca816395f63f95621e66cb9bcf616a8b10e441e0e69f129e22443acb11e77bc1e8170f8e4171b9b7719891efc43699782bfcd4b3a365f01

        GIT_VERSION 1.88.0
        VERSION 1.57
    )

    set(BOOST_NO_HEADERS ${Boost_ADDED})

    if (Boost_ADDED)
        if (MSVC OR ANDROID)
            add_compile_definitions(YUZU_BOOST_v1)
        else()
            message(WARNING "Using bundled Boost on a non-MSVC or Android system is not recommended. You are strongly encouraged to install Boost through your system's package manager.")
        endif()

        if (NOT MSVC)
            # boost sucks
            target_compile_options(boost_heap INTERFACE -Wno-shadow)
            target_compile_options(boost_icl INTERFACE -Wno-shadow)
            target_compile_options(boost_asio INTERFACE -Wno-conversion -Wno-implicit-fallthrough)
        endif()
    endif()

    AddPackage(
        NAME fmt
        REPO fmtlib/fmt
        SHA 40626af88b
        HASH d59f06c24339f223de4ec2afeba1c67b5835a0f350a1ffa86242a72fc3e616a6b8b21798355428d4200c75287308b66634619ffa0b52ba5bd74cc01772ea1a8a
        VERSION 8
        OPTIONS
            "FMT_INSTALL OFF"
    )

    AddPackage(
        NAME lz4
        REPO lz4/lz4
        SHA ebb370ca83
        HASH 43600e87b35256005c0f2498fa56a77de6783937ba4cfce38c099f27c03188d097863e8a50c5779ca0a7c63c29c4f7ed0ae526ec798c1fd2e3736861b62e0a37
        SOURCE_SUBDIR build/cmake
    )

    if (lz4_ADDED)
        add_library(lz4::lz4 ALIAS lz4_static)
    endif()

    AddPackage(
        NAME nlohmann_json
        REPO nlohmann/json
        SHA 55f93686c0
        HASH b739749b066800e21154506ea150d2c5cbce8a45344177f46f884547a1399d26753166fd0df8135269ce28cf223552b1b65cd625b88c844d54753f2434900486
        VERSION 3.8
    )

    AddPackage(
        NAME SimpleIni
        REPO brofield/simpleini
        SHA 09c21bda1d
        HASH 99779ca9b6e040d36558cadf484f9ffdab5b47bcc8fc72e4d33639d1d60c0ceb4410d335ba445d72a4324e455167fd6769d99b459943aa135bec085dff2d4b7c
        EXCLUDE_FROM_ALL ON
    )

    AddPackage(
        NAME ZLIB
        REPO madler/zlib
        SHA 51b7f2abda
        HASH 16eaf1f3752489d12fd9ce30f7b5f7cbd5cb8ff53d617005a9847ae72d937f65e01e68be747f62d7ac19fd0c9aeba9956e60f16d6b465c5fdc2f3d08b4db2e6c
        VERSION 1.2
        OPTIONS
            "ZLIB_BUILD_SHARED OFF"
            "ZLIB_INSTALL OFF"
        EXCLUDE_FROM_ALL ON
    )

    if (ZLIB_ADDED)
        add_library(ZLIB::ZLIB ALIAS zlibstatic)
    endif()

    set(ZSTD_BUILD_SHARED OFF)
    AddPackage(
        NAME zstd
        REPO facebook/zstd
        SHA f8745da6ff
        HASH 3037007f990040fe32573b46f9bef8762fd5dbeeb07ffffcbfeba51ec98167edae39bb9c87f9299efcd61c4e467c5e84f7c19f0df7799bc1fc04864a278792ee
        VERSION 1.5
        SOURCE_SUBDIR build/cmake
        EXCLUDE_FROM_ALL ON
    )

    if (YUZU_TESTS OR DYNARMIC_TESTS)
        AddPackage(
            NAME Catch2
            REPO catchorg/Catch2
            SHA 644821ce28
            HASH f8795f98acf2c02c0db8e734cc866d5caebab4b4a306e93598b97cb3c0c728dafe8283dce27ffe8d42460e5ae7302f3f32e7e274a7f991b73511ac88eef21b1f
            VERSION 3.0.1
        )
    endif()
else()
    # Enforce the search mode of non-required packages for better and shorter failure messages
    find_package(fmt 8 REQUIRED)
    find_package(LLVM MODULE COMPONENTS Demangle)
    find_package(nlohmann_json 3.8 REQUIRED)
    find_package(lz4 REQUIRED)
    find_package(RenderDoc MODULE)
    find_package(SimpleIni MODULE)
    find_package(stb MODULE)
    find_package(ZLIB 1.2 REQUIRED)
    find_package(zstd 1.5 REQUIRED)

    if (YUZU_TESTS)
        find_package(Catch2 3.0.1 REQUIRED)
    endif()

    if (CMAKE_SYSTEM_NAME STREQUAL "Linux" OR ANDROID)
        find_package(gamemode 1.7 MODULE)
    endif()

    if (ENABLE_OPENSSL)
        find_package(OpenSSL 1.1.1 REQUIRED)
    endif()
endif()

if (ENABLE_LIBUSB)
    if (${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
        find_package(libusb MODULE)
    else()
        find_package(libusb 1.0.24 MODULE)
    endif()
endif()

# TODO(crueter): Work around this
if (NOT YUZU_USE_EXTERNAL_VULKAN_SPIRV_TOOLS)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SPIRV-Tools REQUIRED SPIRV-Tools)
endif()

# find SDL2 exports a bunch of variables that are needed, so its easier to do this outside of the YUZU_find_package
# TODO(crueter): combine this all with CPM.
if (ENABLE_SDL2)
    if (YUZU_USE_BUNDLED_SDL2)
        # Detect toolchain and platform
        if ((MSVC_VERSION GREATER_EQUAL 1920) AND ARCHITECTURE_x86_64)
            set(SDL2_VER "SDL2-2.32.8")
        else()
            message(FATAL_ERROR "No bundled SDL2 binaries for your toolchain. Disable YUZU_USE_BUNDLED_SDL2 and provide your own.")
        endif()

        if (DEFINED SDL2_VER)
            download_bundled_external("sdl2/" ${SDL2_VER} "sdl2-bundled" SDL2_PREFIX 2.32.8)
        endif()

        set(SDL2_FOUND YES)
        set(SDL2_INCLUDE_DIR "${SDL2_PREFIX}/include" CACHE PATH "Path to SDL2 headers")
        set(SDL2_LIBRARY "${SDL2_PREFIX}/lib/x64/SDL2.lib" CACHE PATH "Path to SDL2 library")
        set(SDL2_DLL_DIR "${SDL2_PREFIX}/lib/x64/" CACHE PATH "Path to SDL2.dll")

        add_library(SDL2::SDL2 INTERFACE IMPORTED)
        target_link_libraries(SDL2::SDL2 INTERFACE "${SDL2_LIBRARY}")
        target_include_directories(SDL2::SDL2 INTERFACE "${SDL2_INCLUDE_DIR}")
    elseif (YUZU_USE_EXTERNAL_SDL2)
        message(STATUS "Using SDL2 from externals.")
    else()
        find_package(SDL2 2.26.4 REQUIRED)
    endif()
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

add_subdirectory(externals)

# pass targets from externals
find_package(VulkanHeaders)
find_package(VulkanUtilityLibraries)
find_package(VulkanMemoryAllocator)

if (ENABLE_WEB_SERVICE)
    find_package(httplib)
endif()

if (ENABLE_WEB_SERVICE OR ENABLE_QT_UPDATE_CHECKER)
    find_package(cpp-jwt)
endif()

if (NOT YUZU_USE_BUNDLED_SDL2)
    find_package(SDL2 REQUIRED)
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
        find_package(Qt6 REQUIRED COMPONENTS DBus GuiPrivate)
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
    if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
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
    if (USE_DISCORD_PRESENCE)
        list(APPEND YUZU_QT_COMPONENTS2 Network)
    endif()
    set(YUZU_QT_COMPONENTS ${YUZU_QT_COMPONENTS2} PARENT_SCOPE)
endfunction(set_yuzu_qt_components)

if (UNIX AND NOT APPLE AND NOT ANDROID)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBVA libva)
endif()

if (NOT YUZU_USE_BUNDLED_FFMPEG)
    # Use system installed FFmpeg
    #find_package(FFmpeg 4.3 REQUIRED QUIET COMPONENTS ${FFmpeg_COMPONENTS})
    find_package(FFmpeg REQUIRED QUIET COMPONENTS ${FFmpeg_COMPONENTS})

    # TODO(crueter): Version
    set_property(GLOBAL APPEND PROPERTY CPM_PACKAGE_NAMES FFmpeg)
    set_property(GLOBAL APPEND PROPERTY CPM_PACKAGE_SHAS "unknown (system)")
    set_property(GLOBAL APPEND PROPERTY CPM_PACKAGE_URLS "https://github.com/FFmpeg/FFmpeg")
endif()

if(ENABLE_QT)
    set_yuzu_qt_components()
    find_package(Qt6 REQUIRED COMPONENTS ${YUZU_QT_COMPONENTS})
    set(QT_MAJOR_VERSION 6)
    # Qt6 sets cxx_std_17 and we need to undo that
    set_target_properties(Qt6::Platform PROPERTIES INTERFACE_COMPILE_FEATURES "")
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

# Platform-specific library requirements
# ======================================

if (APPLE)
    # Umbrella framework for everything GUI-related
    find_library(COCOA_LIBRARY Cocoa)
    set(PLATFORM_LIBRARIES ${COCOA_LIBRARY} ${IOKIT_LIBRARY} ${COREVIDEO_LIBRARY})
    find_library(ICONV_LIBRARY iconv REQUIRED)
    list(APPEND PLATFORM_LIBRARIES ${ICONV_LIBRARY})
elseif (WIN32)
    # Target Windows 10
    add_definitions(-D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00)
    set(PLATFORM_LIBRARIES winmm ws2_32 iphlpapi)
    if (MINGW)
        # PSAPI is the Process Status API
        set(PLATFORM_LIBRARIES ${PLATFORM_LIBRARIES} psapi imm32 version)
    endif()
elseif (CMAKE_SYSTEM_NAME MATCHES "^(Linux|kFreeBSD|GNU|SunOS)$")
    set(PLATFORM_LIBRARIES rt)
endif()

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

if (YUZU_USE_FASTER_LD AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # We will assume that if the compiler is GCC, it will attempt to use ld.bfd by default.
    # Try to pick a faster linker.
    find_program(LLD lld)
    find_program(MOLD mold)

    if (MOLD AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "12.1")
        message(NOTICE "Selecting mold as linker")
        add_link_options("-fuse-ld=mold")
    elseif (LLD)
        message(NOTICE "Selecting lld as linker")
        add_link_options("-fuse-ld=lld")
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
    install(FILES "dist/org.eden_emu.eden.desktop"
        DESTINATION "share/applications")
    install(FILES "dist/org.eden_emu.eden.svg"
        DESTINATION "share/icons/hicolor/scalable/apps")

    # TODO: these files need to be updated.
    install(FILES "dist/org.eden_emu.eden.xml"
        DESTINATION "share/mime/packages")
    install(FILES "dist/org.eden_emu.eden.metainfo.xml"
        DESTINATION "share/metainfo")
endif()
