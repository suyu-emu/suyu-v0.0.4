# SPDX-FileCopyrightText: Copyright 2025 crueter
# SPDX-License-Identifier: GPL-3.0-or-later

## FasterLinker ##

# This finds a faster linker for your compiler, if available.
# Only really tested on Linux. I would not recommend this on MSYS2.

#[[
    search order:
    - gold (GCC only) - the best, generally, but not packaged anymore
    - mold (GCC only) - generally does well on GCC
    - lld - preferred on clang
    - bfd - the final fallback
    - If none are found (macOS uses ld.prime, etc) just use the default linker
]]

# This module is based on the work of Yuzu, specifically Liam White,
# and later extended by crueter.

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CXX_GCC ON)
endif()

find_program(LINKER_BFD bfd)
if (LINKER_BFD)
    set(LINKER bfd)
endif()

find_program(LINKER_LLD lld)
if (LINKER_LLD)
    set(LINKER lld)
endif()

if (CXX_GCC)
    find_program(LINKER_MOLD mold)
    if (LINKER_MOLD AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "12.1")
        set(LINKER mold)
    endif()

    find_program(LINKER_GOLD gold)
    if (LINKER_GOLD)
        set(LINKER gold)
    endif()
endif()

if (LINKER)
    message(NOTICE "[FasterLinker] Selecting ${LINKER} as linker")
    add_link_options("-fuse-ld=${LINKER}")
else()
    message(WARNING "[FasterLinker] No faster linker found--using default")
endif()

if (LINKER STREQUAL "lld" AND CXX_GCC)
    message(WARNING
        "[FasterLinker] Using lld on GCC may cause issues "
        "with certain LTO settings.")
endif()
