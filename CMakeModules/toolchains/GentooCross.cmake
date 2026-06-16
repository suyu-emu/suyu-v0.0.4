# SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

set(CROSS_TARGET "" CACHE STRING "Cross-compilation target (aarch64, etc)")
set(CROSS_PLATFORM "unknown-linux-gnu" CACHE STRING "Cross-compilation platform (e.g. unknown-linux-gnu)")
set(CROSS_COMPILER "gcc" CACHE STRING "Cross compiler type (gcc or clang)")

if (NOT CROSS_TARGET)
    message(FATAL_ERROR "GentooCross used without a valid CROSS_TARGET")
endif()

set(prefix ${CROSS_TARGET}-${CROSS_PLATFORM})

set(CMAKE_SYSROOT /usr/${prefix})

if (CROSS_COMPILER STREQUAL "gcc")
    set(CMAKE_C_COMPILER ${prefix}-gcc)
    set(CMAKE_CXX_COMPILER ${prefix}-g++)
elseif (CROSS_COMPILER STREQUAL "clang")
    set(CMAKE_C_COMPILER ${prefix}-clang)
    set(CMAKE_CXX_COMPILER ${prefix}-clang++)
else()
    message(FATAL_ERROR "Unsupported cross compiler type ${CROSS_COMPILER}")
endif()

# search programs in the host environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# search headers and libraries in the target environment
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(CMAKE_FIND_ROOT_PATH ${CMAKE_SYSROOT})

# sanity checks
if (NOT IS_DIRECTORY ${CMAKE_SYSROOT})
    message(FATAL_ERROR "Invalid sysroot ${CMAKE_SYSROOT}."
        "Double-check your CROSS_TARGET and CROSS_PLATFORM.")
endif()
