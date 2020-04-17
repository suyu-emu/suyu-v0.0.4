# Definitions for all external bundled libraries

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/CMakeModules)
include(DownloadExternals)

# pkgconfig -- it is used to find shared libraries without cmake modules on linux systems
find_package(PkgConfig)
if (NOT PkgConfig_FOUND)
    function(pkg_check_modules)
        # STUB
    endfunction()
endif()

# Catch
add_library(catch-single-include INTERFACE)
target_include_directories(catch-single-include INTERFACE catch/single_include)

# libfmt
pkg_check_modules(FMT IMPORTED_TARGET GLOBAL fmt>=6.2.0)
if (FMT_FOUND)
    add_library(fmt::fmt ALIAS PkgConfig::FMT)
else()
    message(STATUS "fmt 6.2.0 or newer not found, falling back to externals")
    add_subdirectory(fmt)
    add_library(fmt::fmt ALIAS fmt)
endif()

# Dynarmic
if (ARCHITECTURE_x86_64)
    set(DYNARMIC_TESTS OFF)
    set(DYNARMIC_NO_BUNDLED_FMT ON)
    add_subdirectory(dynarmic)
endif()

# getopt
if (MSVC)
    add_subdirectory(getopt)
endif()

# Glad
add_subdirectory(glad)

# inih
add_subdirectory(inih)

# lz4
pkg_check_modules(LIBLZ4 IMPORTED_TARGET GLOBAL liblz4>=1.8.0)
if (LIBLZ4_FOUND)
    add_library(lz4_static ALIAS PkgConfig::LIBLZ4)
else()
    message(STATUS "liblz4 1.8.0 or newer not found, falling back to externals")
    set(LZ4_BUNDLED_MODE ON)
    add_subdirectory(lz4/contrib/cmake_unofficial EXCLUDE_FROM_ALL)
    target_include_directories(lz4_static INTERFACE ./lz4/lib)
endif()

# mbedtls
add_subdirectory(mbedtls EXCLUDE_FROM_ALL)
target_include_directories(mbedtls PUBLIC ./mbedtls/include)

# MicroProfile
add_library(microprofile INTERFACE)
target_include_directories(microprofile INTERFACE ./microprofile)

# Unicorn
add_library(unicorn-headers INTERFACE)
target_include_directories(unicorn-headers INTERFACE ./unicorn/include)

# Zstandard
pkg_check_modules(LIBZSTD IMPORTED_TARGET GLOBAL libzstd>=1.3.8)
if (LIBZSTD_FOUND)
    add_library(libzstd_static ALIAS PkgConfig::LIBZSTD)
else()
    message(STATUS "libzstd 1.3.8 or newer not found, falling back to externals")
    add_subdirectory(zstd/build/cmake EXCLUDE_FROM_ALL)
    target_include_directories(libzstd_static INTERFACE ./zstd/lib)
endif()

# SoundTouch
add_subdirectory(soundtouch)

# Opus
pkg_check_modules(OPUS IMPORTED_TARGET GLOBAL opus>=1.3.1)
if (OPUS_FOUND)
    add_library(opus ALIAS PkgConfig::OPUS)
else()
    message(STATUS "opus 1.3.1 or newer not found, falling back to externals")
    add_subdirectory(opus)
    target_include_directories(opus INTERFACE ./opus/include)
endif()

# Cubeb
if(ENABLE_CUBEB)
    set(BUILD_TESTS OFF CACHE BOOL "")
    add_subdirectory(cubeb EXCLUDE_FROM_ALL)
endif()

# DiscordRPC
if (USE_DISCORD_PRESENCE)
    add_subdirectory(discord-rpc EXCLUDE_FROM_ALL)
    target_include_directories(discord-rpc INTERFACE ./discord-rpc/include)
endif()

# Sirit
if (ENABLE_VULKAN)
    add_subdirectory(sirit)
endif()

# zlib
find_package(ZLIB 1.2.11)
if (NOT ZLIB_FOUND)
    message(STATUS "zlib 1.2.11 or newer not found, falling back to externals")
    add_subdirectory(zlib EXCLUDE_FROM_ALL)
    set(ZLIB_LIBRARIES z)
endif()

# libzip
pkg_check_modules(LIBZIP IMPORTED_TARGET GLOBAL libzip>=1.5.3)
if (LIBZIP_FOUND)
    add_library(zip ALIAS PkgConfig::LIBZIP)
else()
    message(STATUS "libzip 1.5.3 or newer not found, falling back to externals")
    add_subdirectory(libzip EXCLUDE_FROM_ALL)
endif()

if (ENABLE_WEB_SERVICE)
    # LibreSSL
    find_package(OpenSSL COMPONENTS Crypto SSL)
    if (NOT OpenSSL_FOUND)
        message(STATUS "OpenSSL not found, falling back to externals")
        set(LIBRESSL_SKIP_INSTALL ON CACHE BOOL "")
        add_subdirectory(libressl EXCLUDE_FROM_ALL)
        target_include_directories(ssl INTERFACE ./libressl/include)
        target_compile_definitions(ssl PRIVATE -DHAVE_INET_NTOP)
        get_directory_property(OPENSSL_LIBRARIES
            DIRECTORY libressl
            DEFINITION OPENSSL_LIBS)
    endif()

    # lurlparser
    add_subdirectory(lurlparser EXCLUDE_FROM_ALL)

    # httplib
    add_library(httplib INTERFACE)
    target_include_directories(httplib INTERFACE ./httplib)
    target_compile_definitions(httplib INTERFACE -DCPPHTTPLIB_OPENSSL_SUPPORT)
    target_link_libraries(httplib INTERFACE ${OPENSSL_LIBRARIES})

    # JSON
    add_library(json-headers INTERFACE)
    target_include_directories(json-headers INTERFACE ./json)
endif()
