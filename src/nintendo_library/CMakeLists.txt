# SPDX-FileCopyrightText: 2024 suyu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

add_library(nintendo_library STATIC
    nintendo_library.cpp
    nintendo_library.h
)

target_link_libraries(nintendo_library PUBLIC common)
target_link_libraries(nintendo_library PRIVATE fmt::fmt nlohmann_json::nlohmann_json)

# Use cpp-httplib for HTTP requests (already available in vcpkg)
find_package(httplib CONFIG)
if (httplib_FOUND)
    target_link_libraries(nintendo_library PRIVATE httplib::httplib)
    target_compile_definitions(nintendo_library PRIVATE USE_HTTPLIB)
else()
    # Fallback to CURL if available
    find_package(CURL)
    if (CURL_FOUND)
        target_link_libraries(nintendo_library PRIVATE CURL::libcurl)
        target_compile_definitions(nintendo_library PRIVATE USE_CURL)
    else()
        # No HTTP library available - Nintendo Library features will be disabled
        message(WARNING "Neither cpp-httplib nor CURL found - Nintendo Library network features disabled")
        target_compile_definitions(nintendo_library PRIVATE NO_HTTP_LIBRARY)
    endif()
endif()

if (SUYU_USE_PRECOMPILED_HEADERS)
    target_precompile_headers(nintendo_library PRIVATE nintendo_library.h)
endif()

create_target_directory_groups(nintendo_library)