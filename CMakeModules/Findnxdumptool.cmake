# SPDX-FileCopyrightText: 2024 suyu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

find_path(NXDUMPTOOL_INCLUDE_DIR
    NAMES nxdumptool.h
    HINTS ${CMAKE_SOURCE_DIR}/externals/nxdumptool/nxdumptool
)

find_library(NXDUMPTOOL_LIBRARY
    NAMES nxdumptool
    HINTS ${CMAKE_BINARY_DIR}/externals/nxdumptool
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(nxdumptool DEFAULT_MSG NXDUMPTOOL_LIBRARY NXDUMPTOOL_INCLUDE_DIR)

if(NXDUMPTOOL_FOUND)
    add_library(nxdumptool::nxdumptool INTERFACE IMPORTED)
    target_link_libraries(nxdumptool::nxdumptool INTERFACE ${NXDUMPTOOL_LIBRARY})
    target_include_directories(nxdumptool::nxdumptool INTERFACE ${NXDUMPTOOL_INCLUDE_DIR})
endif()