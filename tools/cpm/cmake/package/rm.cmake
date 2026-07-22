#!/usr/bin/env -S cmake -P

# SPDX-FileCopyrightText: Copyright 2026 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

cmake_minimum_required(VERSION 3.31)

include(${CMAKE_CURRENT_LIST_DIR}/../ScriptUtils.cmake)

function(usage)
    echo([=[
Usage: cpmutil.sh package rm [PACKAGE]...

Delete a package or packages' cpmfile definition.
]=])
endfunction()

set(NO_ALL TRUE)
parse_script_args(args)

get_cpmfile_content(object)

# Remove key
foreach(key ${args})
    string(JSON object REMOVE "${object}" ${key})
endforeach()

# write
get_cpmfile_path(file)
file(WRITE ${file} "${object}")
format_cpmfile()