# SPDX-FileCopyrightText: 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# SPDX-FileCopyrightText: 2016 Citra Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

# TODO(crueter): A lot of this should be moved to the root.
# otherwise we have to do weird shenanigans with library linking and stuff

# cpm
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
        # Fix regset.h collisions
        set(XBYAK_HASH 51f507b0b3)
        set(XBYAK_SHA512SUM 4a29a3c2f97f7d5adf667a21a008be03c951fb6696b0d7ba27e7e4afa037bc76eb5e059bb84860e01baf741d4d3ac851b840cd54c99d038812fbe0f1fa6d38a4)
    else()
        set(XBYAK_HASH 4e44f4614d)
        set(XBYAK_SHA512SUM 5824e92159e07fa36a774aedd3b3ef3541d0241371d522cffa4ab3e1f215fa5097b1b77865b47b2481376c704fa079875557ea463ca63d0a7fd6a8a20a589e70)
    endif()

    AddPackage(
        NAME xbyak
        REPO "Lizzie841/xbyak"
        SHA ${XBYAK_HASH}
        HASH ${XBYAK_SHA512SUM}
        BUNDLED_PACKAGE ON
    )
endif()

# Oaknut (also used by Dynarmic, so needs to be added first)
if (ARCHITECTURE_arm64 OR DYNARMIC_TESTS)
    AddPackage(
        NAME oaknut
        VERSION 2.0.1
        REPO "merryhime/oaknut"
        SHA 94c726ce03
        HASH d8d082242fa1881abce3c82f8dafa002c4e561e66a69e7fc038af67faa5eff2630f082d3d19579c88c4c9f9488e54552accc8cb90e7ce743efe043b6230c08ac
    )
endif()

# getopt
if (MSVC)
    add_subdirectory(getopt)
endif()

# Glad
add_subdirectory(glad)

# mbedtls
AddPackage(
    NAME mbedtls
    REPO "Mbed-TLS/mbedtls"
    SHA "8c88150ca1"
    HASH 769ad1e94c570671071e1f2a5c0f1027e0bf6bcdd1a80ea8ac970f2c86bc45ce4e31aa88d6d8110fc1bed1de81c48bc624df1b38a26f8b340a44e109d784a966
    PATCHES
        ${CMAKE_SOURCE_DIR}/.patch/mbedtls/0001-cmake-version.patch
)

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
AddPackage(
    NAME SPIRV-Headers
    REPO "KhronosGroup/SPIRV-Headers"
    SHA 4e209d3d7e
    HASH f48bbe18341ed55ea0fe280dbbbc0a44bf222278de6e716e143ca1e95ca320b06d4d23d6583fbf8d03e1428f3dac8fa00e5b82ddcd6b425e6236d85af09550a4
)

AddPackage(
    NAME sirit
    REPO "eden-emulator/sirit"
    SHA db1f1e8ab5
    HASH 73eb3a042848c63a10656545797e85f40d142009dfb7827384548a385e1e28e1ac72f42b25924ce530d58275f8638554281e884d72f9c7aaf4ed08690a414b05
    OPTIONS
        "SIRIT_USE_SYSTEM_SPIRV_HEADERS ON"
)

if(MSVC AND USE_CCACHE AND TARGET sirit)
    get_target_property(_opts sirit COMPILE_OPTIONS)
    list(FILTER _opts EXCLUDE REGEX "/Zi")
    list(APPEND _opts "/Z7")
    set_target_properties(sirit PROPERTIES COMPILE_OPTIONS "${_opts}")
endif()

# httplib
if (ENABLE_WEB_SERVICE OR ENABLE_QT_UPDATE_CHECKER)
    AddPackage(
        NAME httplib
        REPO "yhirose/cpp-httplib"
        SHA a609330e4c
        HASH dd3fd0572f8367d8549e1319fd98368b3e75801a293b0c3ac9b4adb806473a4506a484b3d389dc5bee5acc460cb90af7a20e5df705a1696b56496b30b9ce7ed2
        OPTIONS
            "HTTPLIB_REQUIRE_OPENSSL ${ENABLE_OPENSSL}"
    )
endif()

# cpp-jwt
if (ENABLE_WEB_SERVICE)
    AddPackage(
        NAME cpp-jwt
        VERSION 1.4
        REPO "arun11299/cpp-jwt"
        SHA a54fa08a3b
        HASH a90f7e594ada0c7e49d5ff9211c71097534e7742a8e44bf0851b0362642a7271d53f5d83d04eeaae2bad17ef3f35e09e6818434d8eaefa038f3d1f7359d0969a
        FIND_PACKAGE_ARGUMENTS "CONFIG"
        OPTIONS
            "CPP_JWT_BUILD_EXAMPLES OFF"
            "CPP_JWT_BUILD_TESTS OFF"
            "CPP_JWT_USE_VENDORED_NLOHMANN_JSON OFF"
        PATCHES
            ${CMAKE_SOURCE_DIR}/.patch/cpp-jwt/0001-no-install.patch
            ${CMAKE_SOURCE_DIR}/.patch/cpp-jwt/0002-missing-decl.patch
    )
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

AddPackage(
    NAME VulkanHeaders
    VERSION 1.3.274
    REPO "KhronosGroup/Vulkan-Headers"
    SHA 89268a6d17
    HASH 3ab349f74298ba72cafb8561015690c0674d428a09fb91ccd3cd3daca83650d190d46d33fd97b0a8fd4223fe6df2bcabae89136fbbf7c0bfeb8776f9448304c8
    BUNDLED_PACKAGE ${YUZU_USE_EXTERNAL_VULKAN_HEADERS}
)

# Vulkan-Utility-Libraries
AddPackage(
    NAME VulkanUtilityLibraries
    REPO "KhronosGroup/Vulkan-Utility-Libraries"
    SHA df2e358152
    HASH 3e468c3d9ff93f6d418d71e5527abe0a12c8c7ab5b0b52278bbbee4d02bb87e99073906729b727e0147242b7e3fd5dedf68b803f1878cb4c0e4f730bc2238d79
    BUNDLED_PACKAGE ${YUZU_USE_EXTERNAL_VULKAN_UTILITY_LIBRARIES}
)

# SPIRV-Tools
if (YUZU_USE_EXTERNAL_VULKAN_SPIRV_TOOLS)
    AddPackage(
        NAME SPIRV-Tools
        REPO "KhronosGroup/SPIRV-Tools"
        SHA 40eb301f32
        HASH 58d0fb1047d69373cf24c73e6f78c73a72a6cca3b4df1d9f083b9dcc0962745ef154abf3dbe9b3623b835be20c6ec769431cf11733349f45e7568b3525f707aa
        OPTIONS
            "SPIRV_SKIP_EXECUTABLES ON"
    )
endif()

# TZDB (Time Zone Database)
add_subdirectory(nx_tzdb)

# VMA
AddPackage(
    NAME VulkanMemoryAllocator
    REPO "GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator"
    SHA 1076b348ab
    HASH a46b44e4286d08cffda058e856c47f44c7fed3da55fe9555976eb3907fdcc20ead0b1860b0c38319cda01dbf9b1aa5d4b4038c7f1f8fbd97283d837fa9af9772
    FIND_PACKAGE_ARGUMENTS "CONFIG"
)

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

if (ANDROID)
    if (ARCHITECTURE_arm64)
        AddPackage(
            NAME libadrenotools
            REPO "bylaws/libadrenotools"
            SHA 8fae8ce254
            HASH c74fa855f0edebbf25c9bce40b00966daa2447bfc5e15f0cf1a95f86cbf70fc6b02590707edbde16328a0a2a4fb9a1fc419d2dfc22a4a4150971be91892d4edb
            PATCHES
                ${CMAKE_SOURCE_DIR}/.patch/libadrenotools/0001-linkerns-cpm.patch
        )
    endif()
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
    AddPackage(
        NAME oboe
        REPO "google/oboe"
        SHA 2bc873e53c
        HASH 02329058a7f9cf7d5039afaae5ab170d9f42f60f4c01e21eaf4f46073886922b057a9ae30eeac040b3ac182f51b9c1bfe9fe1050a2c9f6ce567a1a9a0ec2c768
        BUNDLED_PACKAGE ON
    )

    add_library(oboe::oboe ALIAS oboe)
endif()

# sse2neon
if (ARCHITECTURE_arm64 AND NOT TARGET sse2neon)
    add_library(sse2neon INTERFACE)
    target_include_directories(sse2neon INTERFACE sse2neon)
endif()
