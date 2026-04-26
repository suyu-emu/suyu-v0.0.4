# SPDX-FileCopyrightText: 2024 suyu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Architecture detection module — sets SUYU_ARCH and SUYU_ARCH_* variables.
# Complements the existing detect_architecture() function in the root CMakeLists.txt
# with additional microarchitecture presets for optimized builds.

if (DEFINED SUYU_ARCH_DETECTED)
    return()
endif()
set(SUYU_ARCH_DETECTED TRUE)

# ── Build preset options ─────────────────────────────────────────────────────
# These define optimization microarchitecture targets (similar to Eden's approach)

set(SUYU_BUILD_PRESET "generic" CACHE STRING "Build preset for CPU architecture optimization")
set_property(CACHE SUYU_BUILD_PRESET PROPERTY STRINGS generic v3 zen2 zen4 native steamdeck apple-m1)

if (SUYU_BUILD_PRESET STREQUAL "v3")
    # x86-64-v3: AVX, AVX2, BMI1/2, F16C, FMA, LZCNT, MOVBE, XSAVE
    message(STATUS "Build preset: x86-64-v3 (AVX2)")
    if (NOT MSVC)
        add_compile_options(-march=x86-64-v3)
    else()
        add_compile_options(/arch:AVX2)
    endif()
elseif (SUYU_BUILD_PRESET STREQUAL "zen2")
    message(STATUS "Build preset: AMD Zen 2")
    if (NOT MSVC)
        add_compile_options(-march=znver2)
    else()
        add_compile_options(/arch:AVX2)
    endif()
elseif (SUYU_BUILD_PRESET STREQUAL "zen4")
    message(STATUS "Build preset: AMD Zen 4")
    if (NOT MSVC)
        add_compile_options(-march=znver4)
    else()
        add_compile_options(/arch:AVX512)
    endif()
elseif (SUYU_BUILD_PRESET STREQUAL "native")
    message(STATUS "Build preset: native (current CPU)")
    if (NOT MSVC)
        add_compile_options(-march=native)
    endif()
elseif (SUYU_BUILD_PRESET STREQUAL "steamdeck")
    message(STATUS "Build preset: Steam Deck (Zen 2, RDNA 2)")
    if (NOT MSVC)
        add_compile_options(-march=znver2 -mtune=znver2)
    else()
        add_compile_options(/arch:AVX2)
    endif()
elseif (SUYU_BUILD_PRESET STREQUAL "apple-m1")
    message(STATUS "Build preset: Apple M1 (ARMv8.4-A)")
    if (NOT MSVC)
        add_compile_options(-mcpu=apple-m1)
    endif()
else()
    message(STATUS "Build preset: generic")
endif()
