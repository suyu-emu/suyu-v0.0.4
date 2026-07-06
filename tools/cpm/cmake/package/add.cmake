#!/usr/bin/env -S cmake -P

# SPDX-FileCopyrightText: Copyright 2026 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

cmake_minimum_required(VERSION 3.31)

include(${CMAKE_CURRENT_LIST_DIR}/../ScriptUtils.cmake)

function(usage)
    echo([=[
Usage: add.cmake [OPTIONS...]

Internal use only.
]=])
endfunction()

if (NOT DEFINED KEY)
    fatal("KEY is required")
endif()

if (NOT DEFINED REPO)
    fatal("REPO is required")
endif()

if (NOT DEFINED VERSION)
    fatal("VERSION is required")
endif()


if (NOT DEFINED CI)
    set(CI FALSE)
endif()

# construct json
set(object "{}")

macro(add key val)
    set(${key} ${val})
    string(JSON object SET ${object} ${key} "\"${val}\"")
endmacro()

add(repo ${REPO})
add(version ${VERSION})

if (DEFINED GIT_HOST AND NOT "${GIT_HOST}" STREQUAL github.com)
    add(git_host ${GIT_HOST})
else()
    set(git_host github.com)
endif()

if (DEFINED PACKAGE)
    add(package ${PACKAGE})
else()
    set(package ${KEY})
endif()

if (DEFINED FIND_ARGS)
    add(find_args "${FIND_ARGS}")
endif()

if (DEFINED MIN_VERSION)
    add(min_version ${MIN_VERSION})
endif()

if (DEFINED ARTIFACT)
    add(artifact ${ARTIFACT})
endif()

if (CI)
    add(ci true)

    if (DEFINED DISABLED_PLATFORMS)
        list_to_array(${DISABLED_PLATFORMS} json_disabled)
        string(JSON object SET "${object}" disabled_platforms "${json_disabled}")
    endif()
else()
    if (DEFINED OPTIONS)
        list_to_array(${OPTIONS} json_options)
        string(JSON object SET "${object}" options "${json_options}")
    endif()

    # get hash
    get_package_url_object(pkg_url)
    get_package_hash("${pkg_url}" pkg_hash)

    add(hash ${pkg_hash})

endif()

echo("\"${KEY}\": ${object}")

# now write
get_cpmfile_content(cpmfile)

# update cached cpmfile content
string(JSON cpmfile SET "${cpmfile}" "${KEY}" "${object}")

# write cached cpmfile
get_cpmfile_path(file)
file(WRITE ${file} "${cpmfile}")
format_cpmfile()

echo("-- Added ${KEY}")
