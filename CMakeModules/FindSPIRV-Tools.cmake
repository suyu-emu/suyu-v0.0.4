# SPDX-FileCopyrightText: 2022 yuzu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)
pkg_search_module(SPIRV-Tools QUIET IMPORTED_TARGET SPIRV-Tools)
find_package_handle_standard_args(SPIRV-Tools
    REQUIRED_VARS SPIRV-Tools_LINK_LIBRARIES
    VERSION_VAR SPIRV-Tools_VERSION
)

if (SPIRV-Tools_FOUND AND NOT TARGET SPIRV-Tools::SPIRV-Tools)
    if (TARGET SPIRV-Tools)
        add_library(SPIRV-Tools::SPIRV-Tools ALIAS SPIRV-Tools)
    else()
        add_library(SPIRV-Tools::SPIRV-Tools ALIAS PkgConfig::SPIRV-Tools)
    endif()
endif()
