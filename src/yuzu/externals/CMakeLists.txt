# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# Explicitly include CPMUtil here since we have a separate cpmfile for Qt externals
set(CPMUTIL_JSON_FILE ${CMAKE_CURRENT_SOURCE_DIR}/cpmfile.json)
include(CPMUtil)

# Disable tests/tools in all externals supporting the standard option name
set(BUILD_TESTING OFF)

# Build only static externals
set(BUILD_SHARED_LIBS OFF)

# Skip install rules for all externals
set_directory_properties(PROPERTIES EXCLUDE_FROM_ALL ON)

# QuaZip
AddJsonPackage(quazip)
