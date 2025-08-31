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
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    set(PLATFORM_LINUX ON)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/externals/cmake-modules")
if (PLATFORM_SUN)
    # Terrific Solaris pkg shenanigans
    list(APPEND CMAKE_PREFIX_PATH "/usr/lib/qt/6.6/lib/amd64/cmake")
    list(APPEND CMAKE_MODULE_PATH "/usr/lib/qt/6.6/lib/amd64/cmake")
    # For some mighty reason, doing a normal release build sometimes may not trigger
    # the proper -O3 switch to materialize
    if (CMAKE_BUILD_TYPE MATCHES "Release")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
    endif()
endif()

set(CPM_SOURCE_CACHE ${CMAKE_SOURCE_DIR}/.cache/cpm)

include(DownloadExternals)
include(CMakeDependentOption)
include(CTest)

# Disable Warnings as Errors for MSVC
if (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3 /WX-")
endif()

if (PLATFORM_FREEBSD)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L/usr/local/lib")
endif()

# Set bundled sdl2/qt as dependent options.
# On Linux system SDL2 is likely to be lacking HIDAPI support which have drawbacks but is needed for SDL motion
CMAKE_DEPENDENT_OPTION(ENABLE_SDL2 "Enable the SDL2 frontend" ON "NOT ANDROID" OFF)

set(EXT_DEFAULT ON)

if (PLATFORM_FREEBSD)
    set(EXT_DEFAULT OFF)
endif()

CMAKE_DEPENDENT_OPTION(YUZU_USE_EXTERNAL_SDL2 "Compile external SDL2" ${EXT_DEFAULT} "ENABLE_SDL2;NOT MSVC" OFF)

cmake_dependent_option(ENABLE_LIBUSB "Enable the use of LibUSB" ON "NOT ANDROID" OFF)

option(ENABLE_OPENGL "Enable OpenGL" ON)
mark_as_advanced(FORCE ENABLE_OPENGL)

option(ENABLE_QT "Enable the Qt frontend" ON)
option(ENABLE_QT_TRANSLATION "Enable translations for the Qt frontend" OFF)
option(ENABLE_QT_UPDATE_CHECKER "Enable update checker for the Qt frontend" OFF)

CMAKE_DEPENDENT_OPTION(YUZU_USE_BUNDLED_QT "Download bundled Qt binaries" "${MSVC}" "ENABLE_QT" OFF)

option(YUZU_USE_CPM "Use CPM to fetch Eden dependencies if needed" ON)

option(ENABLE_WEB_SERVICE "Enable web services (telemetry, etc.)" ON)
option(ENABLE_WIFI_SCAN "Enable WiFi scanning" OFF)

option(YUZU_USE_BUNDLED_FFMPEG "Download/Build bundled FFmpeg" ${EXT_DEFAULT})
option(YUZU_USE_EXTERNAL_VULKAN_HEADERS "Use Vulkan-Headers from externals" ${EXT_DEFAULT})
option(YUZU_USE_EXTERNAL_VULKAN_UTILITY_LIBRARIES "Use Vulkan-Utility-Libraries from externals" ${EXT_DEFAULT})
option(YUZU_USE_EXTERNAL_VULKAN_SPIRV_TOOLS "Use SPIRV-Tools from externals" ${EXT_DEFAULT})

option(YUZU_USE_QT_MULTIMEDIA "Use QtMultimedia for Camera" OFF)

option(YUZU_USE_QT_WEB_ENGINE "Use QtWebEngine for web applet implementation" OFF)

set(YUZU_QT_MIRROR "" CACHE STRING "What mirror to use for downloading the bundled Qt libraries")

option(ENABLE_CUBEB "Enables the cubeb audio backend" ON)

CMAKE_DEPENDENT_OPTION(USE_DISCORD_PRESENCE "Enables Discord Rich Presence" OFF "ENABLE_QT" OFF)

option(ENABLE_MICROPROFILE "Enables microprofile capabilities" OFF)

option(YUZU_TESTS "Compile tests" "${BUILD_TESTING}")

option(YUZU_USE_PRECOMPILED_HEADERS "Use precompiled headers" ${EXT_DEFAULT})

option(YUZU_DOWNLOAD_ANDROID_VVL "Download validation layer binary for android" ON)

option(FORCE_DOWNLOAD_WIN_BUNDLES "Forcefully download bundled Windows dependencies (useful for CI)" OFF)

if (YUZU_USE_CPM AND ENABLE_SDL2)
    option(YUZU_USE_BUNDLED_SDL2 "Download bundled SDL2 build" "${MSVC}")
endif()

CMAKE_DEPENDENT_OPTION(YUZU_ROOM "Enable dedicated room functionality" ON "NOT ANDROID" OFF)

CMAKE_DEPENDENT_OPTION(YUZU_ROOM_STANDALONE "Enable standalone room executable" ON "YUZU_ROOM" OFF)

CMAKE_DEPENDENT_OPTION(YUZU_CMD "Compile the eden-cli executable" ON "NOT ANDROID" OFF)

CMAKE_DEPENDENT_OPTION(YUZU_CRASH_DUMPS "Compile crash dump (Minidump) support" OFF "WIN32 OR LINUX" OFF)

option(YUZU_CHECK_SUBMODULES "Check if submodules are present" ${EXT_DEFAULT})

option(YUZU_ENABLE_LTO "Enable link-time optimization" OFF)

option(YUZU_DOWNLOAD_TIME_ZONE_DATA "Always download time zone binaries" ON)

option(YUZU_ENABLE_PORTABLE "Allow yuzu to enable portable mode if a user folder is found in the CWD" ON)

CMAKE_DEPENDENT_OPTION(YUZU_USE_FASTER_LD "Check if a faster linker is available" ON "NOT WIN32" OFF)

CMAKE_DEPENDENT_OPTION(USE_SYSTEM_MOLTENVK "Use the system MoltenVK lib (instead of the bundled one)" OFF "APPLE" OFF)

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

if (ENABLE_WEB_SERVICE)
    set(DEFAULT_ENABLE_OPENSSL ON)
endif()

option(ENABLE_OPENSSL "Enable OpenSSL backend for ISslConnection" ${DEFAULT_ENABLE_OPENSSL})

if (ENABLE_OPENSSL)
    CMAKE_DEPENDENT_OPTION(YUZU_USE_BUNDLED_OPENSSL "Download bundled OpenSSL build" "${MSVC}" "NOT ANDROID" ON)
endif()

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

if (MSVC AND ARCHITECTURE_x86)
    message(FATAL_ERROR "Attempting to build with the x86 environment is not supported. \
        This can typically happen if you used the Developer Command Prompt from the start menu;\
        instead, run vcvars64.bat directly, located at C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat")
endif()

if (UNIX)
    add_definitions(-DYUZU_UNIX=1)
endif()

if (ARCHITECTURE_arm64 AND (ANDROID OR PLATFORM_LINUX))
    set(HAS_NCE 1)
    add_definitions(-DHAS_NCE=1)
endif()

if (YUZU_ROOM)
    add_definitions(-DYUZU_ROOM)
endif()

# Build/optimization presets
if (PLATFORM_LINUX)
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

include(CPMUtil)

# openssl funniness
if (ENABLE_OPENSSL)
    if (YUZU_USE_BUNDLED_OPENSSL)
        AddJsonPackage(openssl)
    endif()

    find_package(OpenSSL 1.1.1 REQUIRED)
endif()

if (YUZU_USE_CPM)
    message(STATUS "Fetching needed dependencies with CPM")

    set(BUILD_SHARED_LIBS OFF)
    set(BUILD_TESTING OFF)

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
        else()
            message(WARNING "Using bundled Boost on a non-MSVC or Android system is not recommended. You are strongly encouraged to install Boost through your system's package manager.")
        endif()

        if (NOT MSVC)
            # boost sucks
            # Solaris (and probably other NIXes) need explicit pthread definition
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
    endif()

    # Catch2
    if (YUZU_TESTS OR DYNARMIC_TESTS)
        AddJsonPackage(catch2)
    endif()

    # ENet
    AddJsonPackage(enet)

    if (enet_ADDED)
        target_include_directories(enet INTERFACE ${enet_SOURCE_DIR}/include)
    endif()

    # Opus
    AddJsonPackage(opus)
else()
    # Enforce the search mode of non-required packages for better and shorter failure messages
    find_package(fmt 8 REQUIRED)
    find_package(LLVM MODULE COMPONENTS Demangle)
    find_package(nlohmann_json 3.8 REQUIRED)
    find_package(lz4 REQUIRED)
    find_package(RenderDoc MODULE)
    find_package(stb MODULE)
    find_package(enet 1.3 MODULE REQUIRED)
    find_package(Opus 1.3 MODULE REQUIRED)
    find_package(ZLIB 1.2 REQUIRED)
    find_package(zstd 1.5 REQUIRED MODULE)

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

if(NOT TARGET Boost::headers)
    AddJsonPackage(boost_headers)
endif()

if (ENABLE_LIBUSB)
    if (PLATFORM_FREEBSD)
        find_package(libusb MODULE)
    else()
        find_package(libusb 1.0.24 MODULE)
    endif()
endif()

# DiscordRPC
if (USE_DISCORD_PRESENCE)
    AddJsonPackage(discord-rpc)

    target_include_directories(discord-rpc INTERFACE ${discord-rpc_SOURCE_DIR}/include)
    add_library(DiscordRPC::discord-rpc ALIAS discord-rpc)
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
        AddJsonPackage(sdl2)
    endif()

    find_package(SDL2 2.26.4 REQUIRED)
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
find_package(SPIRV-Tools)

if (ENABLE_WEB_SERVICE)
    find_package(httplib)
endif()

if (ENABLE_WEB_SERVICE OR ENABLE_QT_UPDATE_CHECKER)
    find_package(cpp-jwt)
endif()

if (ENABLE_SDL2)
    find_package(SDL2)
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
