#!/usr/bin/env -S cmake -P

# SPDX-FileCopyrightText: Copyright 2026 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

cmake_minimum_required(VERSION 3.31)

include(${CMAKE_CURRENT_LIST_DIR}/../ScriptUtils.cmake)

function(usage)
    echo([=[
Usage: cpmutil.sh package update [-a|--all] [-c|--commit] [PACKAGE]...

Check for updates for a package or packages.

Options:
    -a, --all       Operate on all packages in this project.
    -c, --commit    Automatically generate a commit message
]=])
endfunction()

set(NO_CI TRUE)

parse_script_args(args)

get_cpmfile_content(cpmfile)
set(update_log "")
set(changed FALSE)

foreach(key ${args})
    parse_key(${key})

    get_latest_tag(tag)

    if (${tag} STREQUAL null)
        continue()
    elseif(NOT ${tag} STREQUAL ${version})
        if (numeric_version)
            set(old_version ${numeric_version})
            echo("${key}: ${old_version} -> ${tag}")
            set(numeric_version ${tag})
            get_json_element("${object}" version version)
        else()
            set(old_version ${version})
            echo("${key}: ${old_version} -> ${tag}")
            set(version ${tag})
        endif()

        get_json_element("${object}" artifact artifact)

        # Redo version replacements
        process_version_replacements()

        get_package_url_object(pkg_url)
        get_package_hash("${pkg_url}" pkg_hash)

        if (numeric_version)
            modify_package_numeric("${object}"
                "${tag}" "${pkg_hash}" new_object)
        else()
            modify_package("${object}" "${tag}" "${pkg_hash}" new_object)
        endif()

        # update cached cpmfile content
        string(JSON cpmfile SET "${cpmfile}" "${key}" "${new_object}")

        # used for commit
        string(APPEND update_log "* ${key}: ${old_version} -> ${tag}\n")
        set(changed TRUE)
    else()
        echo("${key}: Up to date")
    endif()
endforeach()

get_cpmfile_path(file)
file(WRITE ${file} "${cpmfile}")
format_cpmfile()

if(MAKE_COMMIT AND changed)
    mktempdir(TMP)

    find_package(Git QUIET)

    if (NOT Git_FOUND)
        fatal("Git is required to be installed for --commit,"
            "but it could not be found.")
    endif()

    set(msg_file ${TMP}/commit_msg.txt)
    file(WRITE ${msg_file} "Update dependencies\n\n${update_log}")
    execute_process(COMMAND ${GIT_EXECUTABLE} add cpmfile.json
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    execute_process(COMMAND ${GIT_EXECUTABLE} commit -F ${msg_file}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    file(REMOVE_RECURSE ${TMP})
endif()
