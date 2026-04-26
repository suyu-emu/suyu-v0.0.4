# SPDX-FileCopyrightText: 2024 suyu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Platform detection module — identifies OS and sets SUYU_PLATFORM_* variables.
# Referenced from Eden's platform detection approach.

if (DEFINED SUYU_PLATFORM_DETECTED)
    return()
endif()
set(SUYU_PLATFORM_DETECTED TRUE)

# ── Detect operating system ──────────────────────────────────────────────────

if (WIN32)
    set(SUYU_PLATFORM_WINDOWS TRUE)
    set(SUYU_PLATFORM_NAME "Windows")
elseif (ANDROID)
    set(SUYU_PLATFORM_ANDROID TRUE)
    set(SUYU_PLATFORM_NAME "Android")
elseif (IOS)
    set(SUYU_PLATFORM_IOS TRUE)
    set(SUYU_PLATFORM_NAME "iOS")
elseif (APPLE)
    set(SUYU_PLATFORM_MACOS TRUE)
    set(SUYU_PLATFORM_NAME "macOS")
elseif (CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    set(SUYU_PLATFORM_FREEBSD TRUE)
    set(SUYU_PLATFORM_NAME "FreeBSD")
elseif (CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
    set(SUYU_PLATFORM_OPENBSD TRUE)
    set(SUYU_PLATFORM_NAME "OpenBSD")
elseif (CMAKE_SYSTEM_NAME STREQUAL "NetBSD")
    set(SUYU_PLATFORM_NETBSD TRUE)
    set(SUYU_PLATFORM_NAME "NetBSD")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Haiku")
    set(SUYU_PLATFORM_HAIKU TRUE)
    set(SUYU_PLATFORM_NAME "Haiku")
elseif (CMAKE_SYSTEM_NAME STREQUAL "SunOS")
    set(SUYU_PLATFORM_SUNOS TRUE)
    set(SUYU_PLATFORM_NAME "SunOS")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(SUYU_PLATFORM_LINUX TRUE)
    set(SUYU_PLATFORM_NAME "Linux")
else()
    set(SUYU_PLATFORM_GENERIC TRUE)
    set(SUYU_PLATFORM_NAME "Generic")
endif()

# ── Aggregate platform categories ────────────────────────────────────────────

# BSD family (FreeBSD, OpenBSD, NetBSD)
if (SUYU_PLATFORM_FREEBSD OR SUYU_PLATFORM_OPENBSD OR SUYU_PLATFORM_NETBSD)
    set(SUYU_PLATFORM_BSD TRUE)
endif()

# UNIX-like (Linux, macOS, BSD, Haiku, SunOS, Android) — but NOT iOS
if (UNIX OR SUYU_PLATFORM_HAIKU OR SUYU_PLATFORM_SUNOS)
    set(SUYU_PLATFORM_UNIX_LIKE TRUE)
endif()

# Desktop platforms (not mobile, not embedded)
if (SUYU_PLATFORM_WINDOWS OR SUYU_PLATFORM_LINUX OR SUYU_PLATFORM_MACOS OR SUYU_PLATFORM_BSD OR SUYU_PLATFORM_HAIKU OR SUYU_PLATFORM_SUNOS)
    set(SUYU_PLATFORM_DESKTOP TRUE)
endif()

# Mobile platforms
if (SUYU_PLATFORM_ANDROID OR SUYU_PLATFORM_IOS)
    set(SUYU_PLATFORM_MOBILE TRUE)
endif()

message(STATUS "Detected platform: ${SUYU_PLATFORM_NAME}")

# ── Platform-specific compiler adjustments ───────────────────────────────────

# FreeBSD/OpenBSD: use libc++ by default when using Clang
if (SUYU_PLATFORM_BSD AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-stdlib=libc++)
    add_link_options(-stdlib=libc++)

    # OpenBSD: disable PSTL backend that requires <execution> from libstdc++
    if (SUYU_PLATFORM_OPENBSD)
        add_compile_definitions(_LIBCPP_HAS_NO_INCOMPLETE_PSTL)
    endif()
endif()

# NetBSD: ensure pkg-config finds the right paths
if (SUYU_PLATFORM_NETBSD)
    list(APPEND CMAKE_PREFIX_PATH "/usr/pkg")
    list(APPEND CMAKE_INCLUDE_PATH "/usr/pkg/include")
    list(APPEND CMAKE_LIBRARY_PATH "/usr/pkg/lib")
endif()

# Haiku: add system paths
if (SUYU_PLATFORM_HAIKU)
    list(APPEND CMAKE_PREFIX_PATH "/boot/system")
endif()

# SunOS: link with socket/nsl libraries
if (SUYU_PLATFORM_SUNOS)
    link_libraries(socket nsl)
endif()

# iOS: set minimum deployment target and disable unsupported features
if (SUYU_PLATFORM_IOS)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0" CACHE STRING "Minimum iOS deployment target")
    set(ENABLE_CUBEB OFF CACHE BOOL "" FORCE)
    set(USE_DISCORD_PRESENCE OFF CACHE BOOL "" FORCE)
endif()
