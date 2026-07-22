#!/usr/bin/env -S cmake -P

# SPDX-FileCopyrightText: Copyright 2026 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

cmake_minimum_required(VERSION 3.31)

include(${CMAKE_CURRENT_LIST_DIR}/../ScriptUtils.cmake)

function(usage)
    echo([=[
Usage: cpmutil.sh package patch [PACKAGE]

Create an in-tree patch for the specified package.
]=])
endfunction()

set(NO_ALL TRUE)

# arg parsing
parse_script_args(args)

list(LENGTH args arg_len)
if(arg_len GREATER 0)
    list(GET args 0 KEY)
endif()

if (NOT KEY)
    fatal("You must provide a key")
endif()

if (NOT DEFINED DESCRIPTION)
    fatal("No description provided")
endif()

parse_key(${KEY})
get_cache_path(${package} ${version} local_cache)

if (NOT EXISTS ${local_cache})
    fatal("${package} is not fetched locally")
endif()

# get last patch number + 1
list(LENGTH patches patches_len)
math(EXPR last_index "${patches_len} - 1")
list(GET patches ${last_index} patch)
string(REGEX MATCH "[0-9][0-9][0-9][0-9]" number "${patch}")
math(EXPR new_patchnum "${number} + 1")

# now pad it for filename usage
set(padded "0000${new_patchnum}")
string(LENGTH ${padded} total_len)
math(EXPR start_index "${total_len} - 4")
string(SUBSTRING ${padded} ${start_index} 4 padded_patch_number)

# this requires Git
find_package(Git REQUIRED)

# fetch temporary package
mktempdir(TMP)

set(CACHE_PATH_OVERRIDE ${TMP}/local)
fetch_package_object()

# make patch dir
set(patch_dir "${CPMUTIL_PATCH_DIR}/${KEY}")
file(MAKE_DIRECTORY "${patch_dir}")

# git stuff
macro(git_cmd out out_status)
    echo("${GIT_EXECUTABLE} ${ARGN}")
    # TODO: error handling
    execute_process(COMMAND ${GIT_EXECUTABLE} ${ARGN}
        WORKING_DIRECTORY ${CACHE_PATH_OVERRIDE}
        OUTPUT_VARIABLE ${out}
        RESULT_VARIABLE ${out_status})
endmacro()

# initialize
git_cmd(_ _ init)
git_cmd(_ _ add -A)
git_cmd(_ _ commit -m init)
git_cmd(_ _ "--work-tree=${local_cache}" add -A)

# check for diffs
git_cmd(_ diff_status diff --cached --quiet)
git_cmd(out _ diff)

if (diff_status EQUAL 0)
    echo(${out})
    echo_error("No differences found between local copy and source")
    file(REMOVE_RECURSE ${TMP})
    cmake_language(EXIT 1)
endif()

# prompt for patch description
git_cmd(_ commit_status commit -m "${DESCRIPTION}")

# format patch
git_cmd(patch_content _ format-patch -1 HEAD --stdout)

# now get patch name...

# strip out existing numeric prefix
string(REGEX REPLACE "^[0-9][0-9][0-9][0-9]-" "" name_part "${DESCRIPTION}")
# spaces to dashes
string(REPLACE " " "-" name_part "${name_part}")
# strip out non-alphanumeric or dash characters
string(REGEX REPLACE "[^a-zA-Z0-9-]" "" name_part "${name_part}")
# collapse consecutive dashes
string(REGEX REPLACE "-+" "-" name_part "${name_part}")
# strip leading/trailing dashes
string(REGEX REPLACE "^-|-$" "" name_part "${name_part}")

if (NOT name_part)
    set(name_part "patch")
    echo_error("Warning: could not determine patch name")
endif()

# Truncate to 49 chars (60 - num prefix - `.patch` suffix)
string(SUBSTRING "${name_part}" 0 49 name_part)

# And remove any trailing dashes
string(REGEX REPLACE "-$" "" name_part "${name_part}")

# now construct and save patch
set(patch_name "${padded_patch_number}-${name_part}.patch")

file(WRITE "${patch_dir}/${patch_name}" "${patch_content}")

echo("-- Patch created at ${patch_dir}/${patch_name}")
file(REMOVE_RECURSE ${TMP})

# Now add to cpmfile.
get_cpmfile_content(content)

# Build new patches JSON array
set(new_patches "[")
if(patches)
    string(JSON existing GET "${content}" "${KEY}" "patches")
    string(JSON len LENGTH "${existing}")
    math(EXPR range_end "${len} - 1")
    foreach(idx RANGE ${range_end})
        string(JSON val GET "${existing}" ${idx})
        string(APPEND new_patches "\"${val}\",")
    endforeach()
endif()
string(APPEND new_patches "\"${patch_name}\"]")

# Update the cpmfile
string(JSON content SET "${content}" "${KEY}" patches "${new_patches}")
get_cpmfile_path(file_path)
file(WRITE "${file_path}" "${content}")
format_cpmfile()

