# SPDX-FileCopyrightText: 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)
pkg_search_module(mbedtls QUIET IMPORTED_TARGET mbedtls)
find_package_handle_standard_args(mbedtls
    REQUIRED_VARS mbedtls_LINK_LIBRARIES
    VERSION_VAR mbedtls_VERSION
)

pkg_search_module(mbedcrypto QUIET IMPORTED_TARGET mbedcrypto)
find_package_handle_standard_args(mbedcrypto
    REQUIRED_VARS mbedcrypto_LINK_LIBRARIES
    VERSION_VAR mbedcrypto_VERSION
)
