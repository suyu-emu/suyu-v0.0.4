#!/usr/bin/env -S cmake -P

# SPDX-FileCopyrightText: Copyright 2026 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

cmake_minimum_required(VERSION 3.31)

include(${CMAKE_CURRENT_LIST_DIR}/../ScriptUtils.cmake)

function(usage)
    echo([=[
Usage: cpmutil.sh package url [-a|--all] [PACKAGE]...

Get the download URL for the specified packages.

Options:
    -a, --all       Operate on all packages in this project.
]=])
endfunction()

parse_script_args(args)

foreach(key ${args})
    parse_key(${key})

    get_package_url_object(pkg_url)
    echo("${key}: ${pkg_url}")
endforeach()