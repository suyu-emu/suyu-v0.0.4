#!/usr/bin/env -S cmake -P

# SPDX-FileCopyrightText: Copyright 2026 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

cmake_minimum_required(VERSION 3.31)

include(${CMAKE_CURRENT_LIST_DIR}/../ScriptUtils.cmake)

function(usage)
    echo([=[
Usage: cpmutil.sh package dir [-a|--all] [PACKAGE]...

Get the local directory for the specified packages.

Options:
    -a, --all       Operate on all packages in this project.
]=])
endfunction()

parse_script_args(args)

# TODO: CI packages.

foreach(key ${args})
    parse_key(${key})

    # Guh.
    get_cache_path(${package} ${version} cache_path)

    cmake_path(ABSOLUTE_PATH cache_path NORMALIZE OUTPUT_VARIABLE abs_path)
    echo("${key}: ${abs_path}")
endforeach()