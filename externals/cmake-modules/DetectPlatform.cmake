# SPDX-FileCopyrightText: Copyright 2025 crueter
# SPDX-License-Identifier: GPL-3.0-or-later

## DetectPlatform ##

# This is a small helper that sets PLATFORM_<platform> variables for various
# operating systems and distributions. Note that Apple, Windows, Android, etc.
# are not covered, as CMake already does that for us.

# It also sets CXX_<compiler> for the C++ compiler.

# Furthermore, some platforms have really silly requirements/quirks, so this
# also does a few of those.

# This module contains contributions from the Eden Emulator Project,
# notably from crueter and Lizzie.

if (${CMAKE_SYSTEM_NAME} STREQUAL "SunOS")
    set(PLATFORM_SUN ON)
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
    set(PLATFORM_FREEBSD ON)
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "OpenBSD")
    set(PLATFORM_OPENBSD ON)
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "NetBSD")
    set(PLATFORM_NETBSD ON)
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "DragonFly")
    set(PLATFORM_DRAGONFLYBSD ON)
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Haiku")
    set(PLATFORM_HAIKU ON)
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    set(PLATFORM_LINUX ON)
endif()

# dumb heuristic to detect msys2
if (CMAKE_COMMAND MATCHES "msys64")
    set(PLATFORM_MSYS ON)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CXX_CLANG ON)
    if (MSVC)
        set(CXX_CLANG_CL ON)
    endif()
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CXX_GCC ON)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(CXX_CL ON)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "IntelLLVM")
    set(CXX_ICC ON)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    set(CXX_APPLE ON)
endif()

# https://gitlab.kitware.com/cmake/cmake/-/merge_requests/11112
# This works totally fine on MinGW64, but not CLANG{,ARM}64
if(MINGW AND CXX_CLANG)
    set(CMAKE_SYSTEM_VERSION 10.0.0)
endif()

# NB: this does not account for SPARC
if (PLATFORM_SUN)
    # Terrific OpenIndiana pkg shenanigans
    list(APPEND CMAKE_PREFIX_PATH
        "${CMAKE_SYSROOT}/usr/lib/qt/6.6/lib/amd64/cmake")
    list(APPEND CMAKE_MODULE_PATH
        "${CMAKE_SYSROOT}/usr/lib/qt/6.6/lib/amd64/cmake")

    # Amazing - absolutely incredible
    list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SYSROOT}/usr/lib/amd64/cmake")
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_SYSROOT}/usr/lib/amd64/cmake")

    # For some mighty reason, doing a normal release build sometimes
    # may not trigger the proper -O3 switch to materialize
    if (CMAKE_BUILD_TYPE MATCHES "Release")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
    endif()
    if (CMAKE_BUILD_TYPE MATCHES "RelWithDebInfo")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
    endif()
endif()

# MSYS2 utilities

# Sometimes, PkgConfig modules will incorrectly reference / when CMake
# wants you to reference it as C:/msys64/. This function corrects that.
# Example in a Find module:
#[[
    if (PLATFORM_MSYS)
        FixMsysPath(PkgConfig::OPUS)
    endif()
]]

function(FixMsysPath target)
    get_target_property(include_dir ${target} INTERFACE_INCLUDE_DIRECTORIES)

    if (NOT (include_dir MATCHES "^/"))
        return()
    endif()

    set(root_default $ENV{MSYS2_LOCATION})
    if (root_default STREQUAL "")
        set(root_default "C:/msys64")
    endif()

    set(MSYS_ROOT_PATH ${root_default}
        CACHE STRING "Location of the MSYS2 root")

    set(include_dir "C:/msys64${include_dir}")
    set_target_properties(${target} PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES ${include_dir})
endfunction()

# MSYSTEM handling + program_path
if (PLATFORM_MSYS)
    # really, really dumb heuristic to detect what environment we are in
    macro(system var)
        if (CMAKE_COMMAND MATCHES ${var})
            set(MSYSTEM ${var})
        endif()
    endmacro()

    system(mingw64)
    system(clang64)
    system(clangarm64)
    system(ucrt64)

    if (NOT DEFINED MSYSTEM)
        set(MSYSTEM msys2)
    endif()

    # We generally want to prioritize environment-specific binaries if possible
    # some, like autoconf, are not present on environments besides msys2 though
    set(CMAKE_PROGRAM_PATH C:/msys64/${MSYSTEM}/bin C:/msys64/usr/bin)
    set(ENV{PKG_CONFIG_PATH} C:/msys64/${MSYSTEM}/lib/pkgconfig)
endif()

# This saves a truly ridiculous amount of time during linking
# In my tests, without this, Eden takes 2 mins, with this, it takes 3-5 seconds
# or on GitHub Actions, 10 minutes -> 3 seconds
if (MINGW)
    set(MINGW_FLAGS "-Wl,--strip-all -Wl,--gc-sections")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE
        "${CMAKE_EXE_LINKER_FLAGS_RELEASE} ${MINGW_FLAGS}")
endif()

# awesome
if (PLATFORM_FREEBSD OR PLATFORM_DRAGONFLYBSD)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L${CMAKE_SYSROOT}/usr/local/lib")
endif()