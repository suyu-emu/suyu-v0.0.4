# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# cpm
include(CPM)

# Disable tests/tools in all externals supporting the standard option name
set(BUILD_TESTING OFF)

# Build only static externals
set(BUILD_SHARED_LIBS OFF)

# Skip install rules for all externals
set_directory_properties(PROPERTIES EXCLUDE_FROM_ALL ON)

# QuaZip
set(CPM_USE_LOCAL_PACKAGES ON)

CPMAddPackage(
    NAME QuaZip-Qt6
    VERSION 1.3
    URL "https://github.com/crueter/quazip-qt6/archive/f838774d63.zip"
    CUSTOM_CACHE_KEY "f838"
)
