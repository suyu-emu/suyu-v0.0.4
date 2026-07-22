# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

function(FixMsysPath target)
    get_target_property(include_dir ${target} INTERFACE_INCLUDE_DIRECTORIES)

    if (NOT (include_dir MATCHES "^/"))
        return()
    endif()

    set(root_default $ENV{MSYS2_LOCATION})
    if (root_default STREQUAL "")
        set(root_default "C:/msys64")
    endif()

    set(MSYS_ROOT_PATH ${root_default} CACHE STRING "Location of the MSYS2 root")

    set(include_dir "C:/msys64${include_dir}")
    set_target_properties(${target} PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES ${include_dir})
endfunction()
