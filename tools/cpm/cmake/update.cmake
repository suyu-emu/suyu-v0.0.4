#!/usr/bin/env -S cmake -P

# SPDX-FileCopyrightText: Copyright 2026 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

cmake_minimum_required(VERSION 3.31)

include(${CMAKE_CURRENT_LIST_DIR}/ScriptUtils.cmake)

# Update CPMUtil and its tooling/etc
set(pwd ${CMAKE_SOURCE_DIR})

set(host "https://git.crueter.xyz")
set(repo "CMake/CPMUtil")
set(release "releases/download/continuous")
set(filename CPMUtil.tar.zst)

mktempdir(TMP)

# Download tarball
set(url "${host}/${repo}/${release}/${filename}")
set(file ${TMP}/${filename})

cpm_download(${url} ${file})

# Extract to current working directory
file(ARCHIVE_EXTRACT
    INPUT ${file}
    DESTINATION ${pwd})

# done :)
echo("Updated CPMUtil")
file(REMOVE_RECURSE ${TMP})