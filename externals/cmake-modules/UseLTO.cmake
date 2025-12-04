# SPDX-FileCopyrightText: Copyright 2025 crueter
# SPDX-License-Identifier: GPL-3.0-or-later

## UseLTO ##

# Enable Interprocedural Optimization (IPO).
# Self-explanatory.

include(CheckIPOSupported)
check_ipo_supported(RESULT COMPILER_SUPPORTS_LTO)
if(NOT COMPILER_SUPPORTS_LTO)
    message(FATAL_ERROR
    "Your compiler does not support interprocedural optimization"
    " (IPO).")
endif()
set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ${COMPILER_SUPPORTS_LTO})
