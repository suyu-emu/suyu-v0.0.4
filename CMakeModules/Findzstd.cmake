# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# SPDX-FileCopyrightText: 2022 yuzu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

include(FindPackageHandleStandardArgs)

find_package(zstd QUIET CONFIG)
if (zstd_CONSIDERED_CONFIGS)
    find_package_handle_standard_args(zstd CONFIG_MODE)
else()
    find_package(PkgConfig QUIET)
    pkg_search_module(ZSTD QUIET IMPORTED_TARGET libzstd)
    find_package_handle_standard_args(zstd
        REQUIRED_VARS ZSTD_LINK_LIBRARIES
        VERSION_VAR ZSTD_VERSION
    )
endif()

if (zstd_FOUND AND NOT TARGET zstd::zstd)
    if (TARGET zstd::libzstd_shared AND NOT YUZU_STATIC_BUILD)
        add_library(zstd::zstd ALIAS zstd::libzstd_shared)
    elseif (TARGET zstd::libzstd_static)
        add_library(zstd::zstd ALIAS zstd::libzstd_static)
    else()
        add_library(zstd::zstd ALIAS PkgConfig::ZSTD)
    endif()
endif()

get_target_property(ZSTD_TARGET zstd::zstd ALIASED_TARGET)

if (NOT TARGET zstd::libzstd)
    if (ZSTD_TARGET)
        add_library(zstd::libzstd ALIAS ${ZSTD_TARGET})
    else()
        add_library(zstd::libzstd ALIAS zstd::zstd)
    endif()
endif()
