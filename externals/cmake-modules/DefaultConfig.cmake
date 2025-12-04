# SPDX-FileCopyrightText: Copyright 2025 crueter
# SPDX-License-Identifier: GPL-3.0-or-later

## DefaultConfig ##

# Generally, you will always want "some" default configuration for your project.
# This module does nothing but enforce that. :)

set(CMAKE_BUILD_TYPE_DEFAULT "Release" CACHE STRING "Default build type")

get_property(IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if (NOT IS_MULTI_CONFIG AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "${CMAKE_BUILD_TYPE_DEFAULT}"
        CACHE STRING "Choose the type of build." FORCE)
    message(STATUS "[DefaultConfig] Defaulting to a "
        "${CMAKE_BUILD_TYPE_DEFAULT} build")
endif()
