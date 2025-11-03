# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

find_package(PkgConfig QUIET)
pkg_search_module(IW QUIET IMPORTED_TARGET iw)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Libiw
    REQUIRED_VARS IW_LIBRARIES IW_INCLUDE_DIRS
    VERSION_VAR IW_VERSION
    FAIL_MESSAGE "libiw (Wireless Tools library) not found. Please install libiw-dev (Debian/Ubuntu), wireless-tools-devel (Fedora), wireless_tools (Arch), or net-wireless/iw (Gentoo)."
)

if (Libiw_FOUND AND TARGET PkgConfig::IW AND NOT TARGET iw::iw)
    add_library(iw::iw ALIAS PkgConfig::IW)
endif()

if(Libiw_FOUND)
    mark_as_advanced(
        IW_INCLUDE_DIRS
        IW_LIBRARIES
        IW_LIBRARY_DIRS
        IW_LINK_LIBRARIES # Often set by pkg-config
        IW_LDFLAGS
        IW_CFLAGS
        IW_CFLAGS_OTHER
        IW_VERSION
    )
endif()
