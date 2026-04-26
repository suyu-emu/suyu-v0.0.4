# SPDX-FileCopyrightText: 2024 suyu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

add_library(nintendo_library STATIC
    nintendo_library.cpp
    nintendo_library.h
)

target_link_libraries(nintendo_library PUBLIC common)
target_link_libraries(nintendo_library PRIVATE fmt::fmt nlohmann_json::nlohmann_json)

# Prefer an existing httplib target from the top-level configuration.
if (NOT TARGET httplib::httplib)
    # Try config package first (vcpkg), then module mode.
    find_package(httplib 0.12 CONFIG QUIET)
endif()
if (NOT TARGET httplib::httplib)
    find_package(httplib 0.12 MODULE QUIET COMPONENTS OpenSSL)
endif()

if (TARGET httplib::httplib)
    target_link_libraries(nintendo_library PRIVATE httplib::httplib)
    target_compile_definitions(nintendo_library PRIVATE USE_HTTPLIB)
else()
    # Fallback to vendored cpp-httplib header-only build if available.
    if (EXISTS "${CMAKE_SOURCE_DIR}/externals/cpp-httplib/httplib.h")
        target_include_directories(nintendo_library PRIVATE
            "${CMAKE_SOURCE_DIR}/externals/cpp-httplib")
        target_compile_definitions(nintendo_library PRIVATE
            USE_HTTPLIB
            CPPHTTPLIB_OPENSSL_SUPPORT)
        if (TARGET OpenSSL::SSL)
            target_link_libraries(nintendo_library PRIVATE OpenSSL::SSL)
        endif()
        if (TARGET OpenSSL::Crypto)
            target_link_libraries(nintendo_library PRIVATE OpenSSL::Crypto)
        endif()
        message(STATUS "Nintendo Library using vendored cpp-httplib")
    else()
        # Fallback to CURL if available
        find_package(CURL QUIET)
        if (CURL_FOUND)
        target_link_libraries(nintendo_library PRIVATE CURL::libcurl)
        target_compile_definitions(nintendo_library PRIVATE USE_CURL)
        else()
            # No HTTP library available - Nintendo Library features will be disabled
            message(WARNING "Neither cpp-httplib nor CURL found - Nintendo Library network features disabled")
            target_compile_definitions(nintendo_library PRIVATE NO_HTTP_LIBRARY)
        endif()
    endif()
endif()

if (SUYU_USE_PRECOMPILED_HEADERS)
    target_precompile_headers(nintendo_library PRIVATE nintendo_library.h)
endif()

create_target_directory_groups(nintendo_library)