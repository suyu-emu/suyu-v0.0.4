# SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.22)

project(yuzu)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/externals/cmake-modules")

set(CPM_SOURCE_CACHE ${CMAKE_SOURCE_DIR}/.cache/cpm)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(DetectPlatform)
include(DetectArchitecture)
include(DefaultConfig)
include(UseLTO)
include(FasterLinker)
include(UseCcache)
include(CMakeDependentOption)
include(CTest)
include(CPMUtil)

if (NOT DEFINED ARCHITECTURE)
    message(FATAL_ERROR "Architecture didn't make it out of scope, did you delete DetectArchitecture.cmake?")
endif()

# Needed for FFmpeg w/ VAAPI and DRM
if (PLATFORM_OPENBSD)
    # OpenBSD 7.8 broke libcxx when upgrading, so we must define the PSTL backend manually
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I${CMAKE_SYSROOT}/usr/X11R6/include -D_LIBCPP_PSTL_BACKEND_SERIAL=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I${CMAKE_SYSROOT}/usr/X11R6/include -D_LIBCPP_PSTL_BACKEND_SERIAL=1")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L${CMAKE_SYSROOT}/usr/X11R6/lib")
elseif (PLATFORM_NETBSD)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I${CMAKE_SYSROOT}/usr/X11R7/include -I${CMAKE_SYSROOT}/usr/pkg/include/c++/v1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I${CMAKE_SYSROOT}/usr/X11R7/include -I${CMAKE_SYSROOT}/usr/pkg/include/c++/v1")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L${CMAKE_SYSROOT}/usr/X11R7/lib")
endif()

# NetBSD: Fun for the whole family!
if (PLATFORM_NETBSD)
    set(ENV{PKG_CONFIG_PATH} "${PKG_CONFIG_PATH}:${CMAKE_SYSROOT}/usr/pkg/lib/ffmpeg7/pkgconfig")
endif()

cmake_dependent_option(YUZU_STATIC_ROOM "Build a static room executable only (CI only)" OFF "PLATFORM_LINUX" OFF)
if (YUZU_STATIC_ROOM)
    set(YUZU_ROOM ON)
    set(YUZU_ROOM_STANDALONE ON)

    # disable e v e r y t h i n g
    set(ENABLE_QT OFF)
    set(ENABLE_SDL2 OFF)
    set(YUZU_CMD OFF)
    set(ENABLE_CUBEB OFF)
    set(ENABLE_UPDATE_CHECKER OFF)
    set(USE_DISCORD_PRESENCE OFF)
    set(BUILD_TESTING OFF)
    set(ENABLE_WEB_SERVICE OFF)
    set(ENABLE_LIBUSB OFF)

    # allow static libs for boost though
    set(Boost_USE_STATIC_LIBS ON)
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    set(OPENSSL_USE_STATIC_LIBS ON)

    set(zstd_FORCE_BUNDLED ON)
    set(fmt_FORCE_BUNDLED ON)
endif()

# qt stuff
option(ENABLE_QT "Enable the Qt frontend" ON)
option(ENABLE_QT_TRANSLATION "Enable translations for the Qt frontend" OFF)
option(ENABLE_UPDATE_CHECKER "Enable update checker (for Qt and Android)" OFF)
option(YUZU_USE_QT_MULTIMEDIA "Use QtMultimedia for Camera" OFF)
cmake_dependent_option(YUZU_USE_QT_WEB_ENGINE "Use QtWebEngine for web applet implementation" OFF "NOT YUZU_USE_BUNDLED_QT" OFF)
set(YUZU_QT_MIRROR "" CACHE STRING "What mirror to use for downloading the bundled Qt libraries")
cmake_dependent_option(YUZU_USE_BUNDLED_QT "Download bundled Qt binaries" "${MSVC}" "ENABLE_QT" OFF)

# non-linux bundled qt are static
if (YUZU_USE_BUNDLED_QT AND (APPLE OR NOT UNIX))
    set(YUZU_STATIC_BUILD ON)
endif()

# TODO: does mingw need any of this anymore
# static stuff
option(YUZU_STATIC_BUILD "Use static libraries and executables if available" OFF)

# TODO: StaticBuild.cmake
if (YUZU_STATIC_BUILD)
    # lol
    set(Boost_USE_STATIC_LIBS ON)
    set(BUILD_SHARED_LIBS OFF)

    ## find .a libs first (static, usually)
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")

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
        set(YUZU_USE_BUNDLED_FFMPEG ON)
        set(YUZU_USE_BUNDLED_SDL2 ON)
        set(YUZU_USE_BUNDLED_OPENSSL ON)

        # these libs do not properly provide static libs/let you do it with cmake
        set(fmt_FORCE_BUNDLED ON)
        set(SPIRV-Tools_FORCE_BUNDLED ON)
        set(SPIRV-Headers_FORCE_BUNDLED ON)
        set(zstd_FORCE_BUNDLED ON)
    endif()
endif()

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
        $<$<COMPILE_LANGUAGE:C,CXX>:/EHsc>)

    # REQUIRED CPU features IN Windows-amd64
    if (ARCHITECTURE_x86_64)
        add_compile_options(
            $<$<COMPILE_LANGUAGE:C,CXX>:-msse4.1>
            $<$<COMPILE_LANGUAGE:C,CXX>:-mcx16>)
    endif()
endif()

# Disable Warnings as Errors for MSVC
if (MSVC AND NOT CXX_CLANG)
    # This was dripping into spirv, being overriden, and causing cl flag override warning
    # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3 /WX-")
    set(CMAKE_CXX_FLAGS_INIT "${CMAKE_CXX_FLAGS_INIT} /W3 /WX-")
endif()

# Set bundled sdl2/qt as dependent options.
# On Linux system SDL2 is likely to be lacking HIDAPI support which have drawbacks but is needed for SDL motion
cmake_dependent_option(ENABLE_SDL2 "Enable the SDL2 frontend" ON "NOT ANDROID" OFF)

# TODO(crueter): Cleanup, each dep that has a bundled option should allow to choose between bundled, external, system
cmake_dependent_option(YUZU_USE_EXTERNAL_SDL2 "Build SDL2 from external source" OFF "ENABLE_SDL2;NOT MSVC" OFF)
cmake_dependent_option(YUZU_USE_BUNDLED_SDL2 "Download bundled SDL2 build" "${MSVC}" "ENABLE_SDL2" OFF)

option(ENABLE_CUBEB "Enables the cubeb audio backend" ON)

set(EXT_DEFAULT OFF)
if (MSVC OR ANDROID)
    set(EXT_DEFAULT ON)
endif()

# ffmpeg
option(YUZU_USE_BUNDLED_FFMPEG "Download bundled FFmpeg" ${EXT_DEFAULT})
cmake_dependent_option(YUZU_USE_EXTERNAL_FFMPEG "Build FFmpeg from external source" "${PLATFORM_SUN}" "NOT WIN32 AND NOT ANDROID" OFF)

# sirit
set(BUNDLED_SIRIT_DEFAULT OFF)
if (MSVC AND NOT (CMAKE_BUILD_TYPE MATCHES "Deb") OR ANDROID)
    set(BUNDLED_SIRIT_DEFAULT ON)
endif()

option(YUZU_USE_BUNDLED_SIRIT "Download bundled sirit" ${BUNDLED_SIRIT_DEFAULT})

# FreeBSD 15+ has libusb, versions below should disable it
cmake_dependent_option(ENABLE_LIBUSB "Enable the use of LibUSB" ON "WIN32 OR PLATFORM_LINUX OR PLATFORM_FREEBSD OR APPLE" OFF)

cmake_dependent_option(ENABLE_OPENGL "Enable OpenGL" ON "NOT WIN32 OR NOT ARCHITECTURE_arm64" OFF)
mark_as_advanced(FORCE ENABLE_OPENGL)

option(ENABLE_WEB_SERVICE "Enable web services (telemetry, etc.)" ON)
option(ENABLE_WIFI_SCAN "Enable WiFi scanning" OFF)

cmake_dependent_option(USE_DISCORD_PRESENCE "Enables Discord Rich Presence" OFF "ENABLE_QT" OFF)

option(YUZU_TESTS "Compile tests" "${BUILD_TESTING}")

option(YUZU_DOWNLOAD_ANDROID_VVL "Download validation layer binary for android" ON)

option(YUZU_LEGACY "Apply patches that improve compatibility with older GPUs (e.g. Snapdragon 865) at the cost of performance" OFF)

option(NIGHTLY_BUILD "Use Nightly qualifiers in the update checker and build metadata" OFF)

cmake_dependent_option(YUZU_ROOM "Enable dedicated room functionality" ON "NOT ANDROID" OFF)
cmake_dependent_option(YUZU_ROOM_STANDALONE "Enable standalone room executable" ON "YUZU_ROOM" OFF)

cmake_dependent_option(YUZU_CMD "Compile the eden-cli executable" ON "ENABLE_SDL2;NOT ANDROID" OFF)

cmake_dependent_option(YUZU_CRASH_DUMPS "Compile crash dump (Minidump) support" OFF "WIN32 OR PLATFORM_LINUX" OFF)

option(YUZU_DOWNLOAD_TIME_ZONE_DATA "Always download time zone binaries" ON)
set(YUZU_TZDB_PATH "" CACHE STRING "Path to a pre-downloaded timezone database")

cmake_dependent_option(YUZU_USE_BUNDLED_MOLTENVK "Download bundled MoltenVK lib" ON "APPLE" OFF)

option(YUZU_DISABLE_LLVM "Disable LLVM (useful for CI)" OFF)

set(DEFAULT_YUZU_USE_BUNDLED_OPENSSL OFF)
if (EXT_DEFAULT OR PLATFORM_SUN OR PLATFORM_OPENBSD)
    set(DEFAULT_YUZU_USE_BUNDLED_OPENSSL ON)
endif()

option(YUZU_USE_BUNDLED_OPENSSL "Download bundled OpenSSL build" ${DEFAULT_YUZU_USE_BUNDLED_OPENSSL})

if (ANDROID AND YUZU_DOWNLOAD_ANDROID_VVL)
    AddJsonPackage(vulkan-validation-layers)

    set(abi ${CMAKE_ANDROID_ARCH_ABI})

    set(vvl_lib_path "${CMAKE_CURRENT_SOURCE_DIR}/src/android/app/src/main/jniLibs/${abi}/")
    file(COPY "${VVL_SOURCE_DIR}/${abi}/libVkLayer_khronos_validation.so"
        DESTINATION "${vvl_lib_path}")
endif()

if(EXISTS ${PROJECT_SOURCE_DIR}/hooks/pre-commit AND NOT EXISTS ${PROJECT_SOURCE_DIR}/.git/hooks/pre-commit)
    if (EXISTS ${PROJECT_SOURCE_DIR}/.git/)
        message(STATUS "Copying pre-commit hook")
        file(COPY hooks/pre-commit DESTINATION ${PROJECT_SOURCE_DIR}/.git/hooks)
    endif()
endif()

set(compat_base dist/compatibility_list/compatibility_list)
set(compat_qrc ${compat_base}.qrc)
set(compat_json ${compat_base}.json)

configure_file(${PROJECT_SOURCE_DIR}/${compat_qrc}
    ${PROJECT_BINARY_DIR}/${compat_qrc}
    COPYONLY)

if (EXISTS ${PROJECT_SOURCE_DIR}/${compat_json})
    configure_file("${PROJECT_SOURCE_DIR}/${compat_json}"
        "${PROJECT_BINARY_DIR}/${compat_json}"
        COPYONLY)
endif()

# TODO: Compat list download
if (NOT EXISTS ${PROJECT_BINARY_DIR}/${compat_json})
    file(WRITE ${PROJECT_BINARY_DIR}/${compat_json} "")
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
if (CXX_GCC OR CXX_CLANG)
    if (ARCHITECTURE_x86_64)
        # See https://gcc.gnu.org/onlinedocs/gcc/x86-Options.html
        # Generic supports Pentium Pro instruction set and above
        # TODO: if a value is unknown, pass that as march and mtune
        set(YUZU_BUILD_PRESET "custom" CACHE STRING "Build preset to use. One of: custom, generic, v3, zen2, zen4, native")

        if (${YUZU_BUILD_PRESET} STREQUAL "generic")
            set(march x86-64)
            set(mtune generic)
        elseif (${YUZU_BUILD_PRESET} STREQUAL "v3")
            set(march x86-64-v3)
            set(mtune generic)
        elseif (${YUZU_BUILD_PRESET} STREQUAL "zen2")
            set(march znver2)
            set(mtune znver2)
        elseif (${YUZU_BUILD_PRESET} STREQUAL "zen4")
            set(march znver4)
            set(mtune znver4)
        endif()
    elseif(ARCHITECTURE_arm64)
        # See https://gcc.gnu.org/onlinedocs/gcc/AArch64-Options.html
        set(YUZU_BUILD_PRESET "custom" CACHE STRING "Build preset to use. One of: custom, generic, armv9, native")
        set(mtune generic)

        if (${YUZU_BUILD_PRESET} STREQUAL "generic")
            set(march armv8-a)
        elseif (${YUZU_BUILD_PRESET} STREQUAL "armv9")
            set(march armv9-a)
        endif()
    endif()

    if ("${YUZU_BUILD_PRESET}" STREQUAL "native")
        set(march native)
        set(mtune native)
    endif()

    if (DEFINED march AND DEFINED mtune)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=${march} -mtune=${mtune}")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=${march} -mtune=${mtune}")
    endif()
endif()

# Other presets, e.g. steamdeck
# TODO(crueter): Just have every Linux/Windows use old sdl2
set(YUZU_SYSTEM_PROFILE "generic" CACHE STRING "CMake and Externals profile to use. One of: generic, steamdeck")

# Configure C++ standard
# ===========================

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output binaries to bin/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

# System imported libraries
# =======================================================================

# Prefer the -pthread flag on Linux.
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

find_package(RenderDoc MODULE)

# openssl funniness
if (YUZU_USE_BUNDLED_OPENSSL)
    set(BUILD_SHARED_LIBS OFF)
    AddJsonPackage(openssl)
    if (OpenSSL_ADDED)
        add_compile_definitions(YUZU_BUNDLED_OPENSSL)
    endif()
endif()

find_package(OpenSSL 3 REQUIRED)

message(STATUS "Fetching needed dependencies with CPM")

set(BUILD_SHARED_LIBS OFF)
set(BUILD_TESTING OFF)
set(ENABLE_TESTING OFF)

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

# zstd
AddJsonPackage(zstd)

if (zstd_ADDED)
    add_library(zstd::zstd ALIAS libzstd_static)
    add_library(zstd::libzstd ALIAS libzstd_static)
endif()

if (NOT YUZU_STATIC_ROOM)
    # nlohmann
    AddJsonPackage(nlohmann)

    # zlib
    AddJsonPackage(zlib)

    if (ZLIB_ADDED)
        add_library(ZLIB::ZLIB ALIAS zlibstatic)
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
    find_library(COCOA_LIBRARY Cocoa REQUIRED)
    find_library(IOKIT_LIBRARY IOKit REQUIRED)
    set(PLATFORM_LIBRARIES ${COCOA_LIBRARY} ${IOKIT_LIBRARY} ${COREVIDEO_LIBRARY})
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
    set(PLATFORM_LIBRARIES bsd /boot/system/lib/libnetwork.so)
elseif (CMAKE_SYSTEM_NAME MATCHES "^(Linux|kFreeBSD|GNU|SunOS)$")
    set(PLATFORM_LIBRARIES rt)
endif()

message(STATUS "Platform Libraries: ${PLATFORM_LIBRARIES}")

add_subdirectory(externals)

# pass targets from externals
# TODO(crueter): CPMUtil Propagate func?
find_package(enet)
find_package(unordered_dense REQUIRED)

if (ARCHITECTURE_x86 OR ARCHITECTURE_x86_64)
    find_package(xbyak)
endif()

if (NOT YUZU_STATIC_ROOM)
    find_package(libusb)
    find_package(VulkanMemoryAllocator)
    find_package(VulkanUtilityLibraries)
    find_package(SimpleIni)
    find_package(SPIRV-Tools)
    find_package(sirit)
    find_package(gamemode)
    find_package(mcl)
    find_package(frozen)

    if (ARCHITECTURE_riscv64)
        find_package(biscuit)
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
endif()

# Qt stuff
if (ENABLE_QT)
    if (YUZU_USE_BUNDLED_QT)
        # Qt 6.8+ is broken on macOS (??)
        if (APPLE)
            AddQt(6.7.3)
        else()
            AddQt(6.9.3)
        endif()

        set(YUZU_STATIC_BUILD ON)
    else()
        message(STATUS "Using system Qt")
        if (NOT Qt6_DIR)
            set(Qt6_DIR "" CACHE PATH "Additional path to search for Qt6 libraries like C:/Qt/6.8.3/msvc2022_64/lib/cmake/Qt6")
        endif()
        list(APPEND CMAKE_PREFIX_PATH "${Qt6_DIR}")
    endif()

    find_package(Qt6 CONFIG REQUIRED COMPONENTS Widgets Charts Concurrent)

    if (YUZU_USE_QT_MULTIMEDIA)
        find_package(Qt6 REQUIRED COMPONENTS Multimedia)
    endif()

    if (PLATFORM_LINUX OR PLATFORM_FREEBSD)
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

    ## Components ##

    # Best practice is to ask for all components at once, so they are from the same version
    set(YUZU_QT_COMPONENTS Core Widgets Charts Concurrent)
    if (PLATFORM_LINUX)
        list(APPEND YUZU_QT_COMPONENTS DBus)
    endif()
    if (YUZU_USE_QT_MULTIMEDIA)
        list(APPEND YUZU_QT_COMPONENTS Multimedia)
    endif()
    if (YUZU_USE_QT_WEB_ENGINE)
        list(APPEND YUZU_QT_COMPONENTS WebEngineCore WebEngineWidgets)
    endif()
    if (ENABLE_QT_TRANSLATION)
        list(APPEND YUZU_QT_COMPONENTS LinguistTools)
    endif()

    find_package(Qt6 REQUIRED COMPONENTS ${YUZU_QT_COMPONENTS})
    set(QT_MAJOR_VERSION 6)
    # Qt6 sets cxx_std_17 and we need to undo that
    set_target_properties(Qt6::Platform PROPERTIES INTERFACE_COMPILE_FEATURES "")

    ## Qt Externals ##

    # QuaZip
    AddJsonPackage(quazip)
endif()

if (NOT YUZU_STATIC_ROOM AND NOT (YUZU_USE_BUNDLED_FFMPEG OR YUZU_USE_EXTERNAL_FFMPEG))
    # Use system installed FFmpeg
    find_package(FFmpeg REQUIRED QUIET COMPONENTS ${FFmpeg_COMPONENTS})

    # TODO(crueter): Version
    set_property(GLOBAL APPEND PROPERTY CPM_PACKAGE_NAMES FFmpeg)
    set_property(GLOBAL APPEND PROPERTY CPM_PACKAGE_SHAS "unknown (system)")
    set_property(GLOBAL APPEND PROPERTY CPM_PACKAGE_URLS "https://github.com/FFmpeg/FFmpeg")
endif()

if (WIN32 AND YUZU_CRASH_DUMPS)
    message(STATUS "YUZU_CRASH_DUMPS is unimplemented on Windows. Check back later.")
    # set(BREAKPAD_VER "breakpad-c89f9dd")
    # download_bundled_external("breakpad/" ${BREAKPAD_VER} "breakpad-win" BREAKPAD_PREFIX "c89f9dd")

    # set(BREAKPAD_CLIENT_INCLUDE_DIR "${BREAKPAD_PREFIX}/include")
    # set(BREAKPAD_CLIENT_LIBRARY "${BREAKPAD_PREFIX}/lib/libbreakpad_client.lib")

    # add_library(libbreakpad_client INTERFACE IMPORTED)
    # target_link_libraries(libbreakpad_client INTERFACE "${BREAKPAD_CLIENT_LIBRARY}")
    # target_include_directories(libbreakpad_client INTERFACE "${BREAKPAD_CLIENT_INCLUDE_DIR}")
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
        IMPORTED_LOCATION "${LIB_PATH}")

    link_libraries(llvm-mingw-runtime)
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
