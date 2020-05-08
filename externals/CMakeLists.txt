# Definitions for all external bundled libraries

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/CMakeModules")
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/externals/find-modules")
include(DownloadExternals)

# Catch
add_library(catch-single-include INTERFACE)
target_include_directories(catch-single-include INTERFACE catch/single_include)

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

# mbedtls
add_subdirectory(mbedtls EXCLUDE_FROM_ALL)
target_include_directories(mbedtls PUBLIC ./mbedtls/include)

# MicroProfile
add_library(microprofile INTERFACE)
target_include_directories(microprofile INTERFACE ./microprofile)

# Unicorn
add_library(unicorn-headers INTERFACE)
target_include_directories(unicorn-headers INTERFACE ./unicorn/include)

# SoundTouch
add_subdirectory(soundtouch)

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

# libzip
find_package(Libzip 1.5)
if (NOT LIBZIP_FOUND)
    message(STATUS "libzip 1.5 or newer not found, falling back to externals")
    add_subdirectory(libzip EXCLUDE_FROM_ALL)
endif()

if (ENABLE_WEB_SERVICE)
    # lurlparser
    add_subdirectory(lurlparser EXCLUDE_FROM_ALL)

    # httplib
    add_library(httplib INTERFACE)
    target_include_directories(httplib INTERFACE ./httplib)
    target_compile_definitions(httplib INTERFACE -DCPPHTTPLIB_OPENSSL_SUPPORT)
    target_link_libraries(httplib INTERFACE OpenSSL::SSL OpenSSL::Crypto)
endif()
