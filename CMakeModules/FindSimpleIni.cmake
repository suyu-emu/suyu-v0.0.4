# SPDX-FileCopyrightText: 2023 Alexandre Bouvier <contact@amb.tf>
#
# SPDX-License-Identifier: GPL-3.0-or-later

find_path(SimpleIni_INCLUDE_DIR SimpleIni.h)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SimpleIni
    REQUIRED_VARS SimpleIni_INCLUDE_DIR
)

if (SimpleIni_FOUND AND NOT TARGET SimpleIni::SimpleIni)
    add_library(SimpleIni::SimpleIni INTERFACE IMPORTED)
    set_target_properties(SimpleIni::SimpleIni PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${SimpleIni_INCLUDE_DIR}"
    )
endif()

mark_as_advanced(SimpleIni_INCLUDE_DIR)
