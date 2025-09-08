# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

include(CPMUtil)

# Always build externals as static libraries, even when dynarmic is built as shared
set(BUILD_SHARED_LIBS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set_property(DIRECTORY PROPERTY EXCLUDE_FROM_ALL ON)

# Allow options shadowing with normal variables when subproject use old cmake policy
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

# Disable tests in all externals supporting the standard option name
set(BUILD_TESTING OFF)

# For libraries that already come with a CMakeLists file,
# simply add the directory to that file as a subdirectory
# to have CMake automatically recognize them.

# biscuit

if ("riscv" IN_LIST ARCHITECTURE)
    AddJsonPackage(
        NAME biscuit
        BUNDLED_PACKAGE ${DYNARMIC_USE_BUNDLED_EXTERNALS}
    )
endif()

# mcl
AddJsonPackage(
    NAME mcl
    BUNDLED_PACKAGE ${DYNARMIC_USE_BUNDLED_EXTERNALS}
)

# TODO(crueter): maybe it's just Gentoo but zydis system package really sucks
if ("x86_64" IN_LIST ARCHITECTURE)
    set(CMAKE_DISABLE_FIND_PACKAGE_Doxygen ON)
    AddJsonPackage(
        NAME zycore
        BUNDLED_PACKAGE ${DYNARMIC_USE_BUNDLED_EXTERNALS}
    )

    AddJsonPackage(
        NAME zydis
        BUNDLED_PACKAGE ${DYNARMIC_USE_BUNDLED_EXTERNALS}
    )
endif()
