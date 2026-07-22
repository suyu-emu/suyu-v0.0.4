#!/usr/bin/env -S cmake -P

# SPDX-FileCopyrightText: Copyright 2026 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

cmake_minimum_required(VERSION 3.31)

include(${CMAKE_CURRENT_LIST_DIR}/../ScriptUtils.cmake)

function(usage)
    echo([=[
Usage: cpmutil.sh package reset [-a|--all] [PACKAGE]...

Reset a locally fetched package to its original state.
This is most useful for dropping any changes you've made.

Options:
    -a, --all       Operate on all packages in this project.
]=])
endfunction()

set(NO_CI TRUE)
parse_script_args(args)

foreach(key ${args})
    if (ci)
        continue()
    endif()

    parse_key(${key})
    echo("-- ${key}")

    fetch_package_object(FORCE)
endforeach()
