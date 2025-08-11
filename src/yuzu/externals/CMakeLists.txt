# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# cpm
include(CPMUtil)

# Disable tests/tools in all externals supporting the standard option name
set(BUILD_TESTING OFF)

# Build only static externals
set(BUILD_SHARED_LIBS OFF)

# Skip install rules for all externals
set_directory_properties(PROPERTIES EXCLUDE_FROM_ALL ON)

# QuaZip
AddPackage(
    NAME QuaZip-Qt6
    VERSION 1.3
    REPO "crueter/quazip-qt6"
    SHA f838774d63
    HASH 9f629a438699801244a106c8df6d5f8f8d19e80df54f530a89403a10c8c4e37a6e95606bbdd307f23636961e8ce34eb37a2186d589a1f227ac9c8e2c678e326e
    SYSTEM_PACKAGE ON
)
