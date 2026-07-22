#!/usr/bin/env -S cmake -P

# SPDX-FileCopyrightText: Copyright 2026 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

cmake_minimum_required(VERSION 3.31)

include(${CMAKE_CURRENT_LIST_DIR}/ScriptUtils.cmake)

set(NO_ALL TRUE)

function(usage)
    echo([=[
Usage: cpmutil.sh ls

List all packages in the cpmfile.
]=])
endfunction()

get_cpmfile_keys(keys)

foreach(key ${keys})
    echo("${key}")
endforeach()
