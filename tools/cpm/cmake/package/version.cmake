#!/usr/bin/env -S cmake -P

# SPDX-FileCopyrightText: Copyright 2026 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

cmake_minimum_required(VERSION 3.31)

include(${CMAKE_CURRENT_LIST_DIR}/../ScriptUtils.cmake)

function(usage)
    echo([=[
Usage: cpmutil.sh package version [PACKAGE] [VERSION]

Update a package's version. If the package uses a sha, you must provide a sha,
and if the package uses a tag, you must provide the fully qualified tag.
]=])
endfunction()

set(NO_ALL TRUE)
# arg parsing
parse_script_args(args)

list(LENGTH args arg_len)
if(arg_len GREATER 0)
    list(GET args 0 KEY)
endif()

if(arg_len GREATER 1)
    list(GET args 1 NEW_VERSION)
endif()

# checks
if (NOT KEY)
    fatal("You must provide a key")
endif()

if (NOT NEW_VERSION)
    fatal("You must provide a version")
endif()

# action
get_cpmfile_content(cpmfile)
parse_key("${KEY}")

if (numeric_version)
    set(numeric_version ${NEW_VERSION})
    get_json_element("${object}" version version)
else()
    set(version ${NEW_VERSION})
endif()

get_json_element("${object}" artifact artifact)

# Redo version replacements
process_version_replacements()

get_package_url_object(pkg_url)
get_package_hash("${pkg_url}" pkg_hash)

if (numeric_version)
    modify_package_numeric("${object}"
        "${NEW_VERSION}" "${pkg_hash}" new_object)
else()
    modify_package("${object}" "${NEW_VERSION}" "${pkg_hash}" new_object)
endif()

# update cached cpmfile content
string(JSON cpmfile SET "${cpmfile}" "${key}" "${new_object}")

# write cached cpmfile
get_cpmfile_path(file)
file(WRITE ${file} "${cpmfile}")
format_cpmfile()

echo("-- * Updated")
