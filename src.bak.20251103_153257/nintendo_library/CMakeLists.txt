# SPDX-FileCopyrightText: 2024 suyu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

add_library(nintendo_library STATIC
    nintendo_library.cpp
    nintendo_library.h
)

target_link_libraries(nintendo_library PUBLIC common)
target_link_libraries(nintendo_library PRIVATE fmt::fmt nlohmann_json::nlohmann_json)

# Add CURL for HTTP requests
find_package(CURL REQUIRED)
target_link_libraries(nintendo_library PRIVATE CURL::libcurl)

if (SUYU_USE_PRECOMPILED_HEADERS)
    target_precompile_headers(nintendo_library PRIVATE nintendo_library.h)
endif()

create_target_directory_groups(nintendo_library)