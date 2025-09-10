# SPDX-FileCopyrightText: 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)
pkg_search_module(sirit QUIET IMPORTED_TARGET sirit)
find_package_handle_standard_args(sirit
    REQUIRED_VARS sirit_LINK_LIBRARIES
    VERSION_VAR sirit_VERSION
)
