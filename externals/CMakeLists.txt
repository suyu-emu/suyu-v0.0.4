# SPDX-FileCopyrightText: 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# SPDX-FileCopyrightText: 2016 Citra Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

# TODO(crueter): A lot of this should be moved to the root.
# otherwise we have to do weird shenanigans with library linking and stuff

# Explicitly include CPMUtil here since we have a separate cpmfile for externals
set(CPMUTIL_JSON_FILE ${CMAKE_CURRENT_SOURCE_DIR}/cpmfile.json)
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

if (FORCE_DOWNLOAD_WIN_BUNDLES)
    download_win_archives()
endif()

# Xbyak (also used by Dynarmic, so needs to be added first)
if (ARCHITECTURE_x86 OR ARCHITECTURE_x86_64)
    if (PLATFORM_SUN)
        AddJsonPackage(xbyak_sun)
    else()
        AddJsonPackage(xbyak)
    endif()
endif()

# Oaknut (also used by Dynarmic, so needs to be added first)
if (ARCHITECTURE_arm64 OR DYNARMIC_TESTS)
    AddJsonPackage(oaknut)
endif()

# getopt
if (MSVC)
    add_subdirectory(getopt)
endif()

# Glad
add_subdirectory(glad)

# mbedtls
AddJsonPackage(mbedtls)

if (mbedtls_ADDED)
    target_include_directories(mbedtls PUBLIC ${mbedtls_SOURCE_DIR}/include)

    if (NOT MSVC)
        target_compile_options(mbedcrypto PRIVATE
            -Wno-unused-but-set-variable
            -Wno-string-concatenation)
    endif()
endif()

# libusb
if (ENABLE_LIBUSB AND NOT TARGET libusb::usb)
    add_subdirectory(libusb)
endif()

# Sirit
# TODO(crueter): spirv-tools doesn't work w/ system
set(SPIRV_WERROR OFF)
AddJsonPackage(spirv-headers)

AddJsonPackage(sirit)

if(MSVC AND USE_CCACHE AND sirit_ADDED)
    get_target_property(_opts sirit COMPILE_OPTIONS)
    list(FILTER _opts EXCLUDE REGEX "/Zi")
    list(APPEND _opts "/Z7")
    set_target_properties(sirit PROPERTIES COMPILE_OPTIONS "${_opts}")
endif()

# httplib
if (ENABLE_WEB_SERVICE OR ENABLE_QT_UPDATE_CHECKER)
    AddJsonPackage(httplib)
endif()

# cpp-jwt
if (ENABLE_WEB_SERVICE)
    AddJsonPackage(cpp-jwt)
endif()

# unordered_dense
AddPackage(
    NAME unordered_dense
    REPO "Lizzie841/unordered_dense"
    SHA e59d30b7b1
    HASH 71eff7bd9ba4b9226967bacd56a8ff000946f8813167cb5664bb01e96fb79e4e220684d824fe9c59c4d1cc98c606f13aff05b7940a1ed8ab3c95d6974ee34fa0
    FIND_PACKAGE_ARGUMENTS "CONFIG"
    OPTIONS
        "UNORDERED_DENSE_INSTALL OFF"
)

# FFMpeg
if (YUZU_USE_BUNDLED_FFMPEG)
    add_subdirectory(ffmpeg)
    set(FFmpeg_PATH "${FFmpeg_PATH}" PARENT_SCOPE)
    set(FFmpeg_LDFLAGS "${FFmpeg_LDFLAGS}" PARENT_SCOPE)
    set(FFmpeg_LIBRARIES "${FFmpeg_LIBRARIES}" PARENT_SCOPE)
    set(FFmpeg_INCLUDE_DIR "${FFmpeg_INCLUDE_DIR}" PARENT_SCOPE)
endif()

# Vulkan-Headers

# TODO(crueter): Vk1.4 impl

AddJsonPackage(
    NAME vulkan-headers
    BUNDLED_PACKAGE ${YUZU_USE_EXTERNAL_VULKAN_HEADERS}
)

# Vulkan-Utility-Libraries
AddJsonPackage(
    NAME vulkan-utility-libraries
    BUNDLED_PACKAGE ${YUZU_USE_EXTERNAL_VULKAN_UTILITY_LIBRARIES}
)

# SPIRV Tools
AddJsonPackage(
    NAME spirv-tools
    BUNDLED_PACKAGE ${YUZU_USE_EXTERNAL_VULKAN_SPIRV_TOOLS}
)

if (SPIRV-Tools_ADDED)
    add_library(SPIRV-Tools::SPIRV-Tools ALIAS SPIRV-Tools-static)
    target_link_libraries(SPIRV-Tools-static PRIVATE SPIRV-Tools-opt SPIRV-Tools-link)
endif()

# TZDB (Time Zone Database)
add_subdirectory(nx_tzdb)

# VMA
AddJsonPackage(vulkan-memory-allocator)

if (VulkanMemoryAllocator_ADDED AND MSVC)
    target_compile_options(VulkanMemoryAllocator INTERFACE /wd4189)
endif()

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
        target_compile_definitions(libbreakpad_client PUBLIC -DHAVE_A_OUT_H)
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

# sse2neon
if (ARCHITECTURE_arm64 AND NOT TARGET sse2neon)
    add_library(sse2neon INTERFACE)
    target_include_directories(sse2neon INTERFACE sse2neon)
endif()
