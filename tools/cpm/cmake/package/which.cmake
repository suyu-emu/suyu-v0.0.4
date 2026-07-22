#!/usr/bin/env -S cmake -P

# SPDX-FileCopyrightText: Copyright 2026 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

cmake_minimum_required(VERSION 3.31)

include(${CMAKE_CURRENT_LIST_DIR}/../ScriptUtils.cmake)

function(usage)
    echo([=[
Usage: cpmutil.sh package which [PACKAGE]...

Check if a package or packages are defined in the cpmfile.
]=])
endfunction()

set(NO_ALL TRUE)
parse_script_args(args)
set(exit 0)

get_cpmfile_content(object)

foreach(key ${args})
    # Check if a key exists
    string(JSON member ERROR_VARIABLE err GET "${object}" ${key})

    if (NOT err)
        echo("${key}")
    else()
        echo_error("${key} not defined in cpmfile")
        set(exit 1)
    endif()
endforeach()

cmake_language(EXIT ${exit})