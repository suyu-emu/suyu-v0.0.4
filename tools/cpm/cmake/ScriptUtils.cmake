#!/usr/bin/env -S cmake -P

# SPDX-FileCopyrightText: Copyright 2026 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

cmake_minimum_required(VERSION 3.31)

# TODO: Account for CPMConfig.cmake
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR})
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/CMakeModules)
set(CPMUTIL_ROOT ${CMAKE_SOURCE_DIR})

include(CPMUtil)

# Parse the JSON object for a given key.
macro(parse_key key)
    get_json_object(${key})
    set(JSON_NAME ${key})
    parse_object(${object})
endmacro()


# Analogous to GNU mktemp, with fallbacks
function(mktempdir out)
    # shell out to system mktemp if available
    cpm_find_program(MKTEMP_EXECUTABLE mktemp)
    if (MKTEMP_EXECUTABLE)
        execute_process(COMMAND mktemp -d
            OUTPUT_VARIABLE dir
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE ret)

        if (ret EQUAL 0)
            set(${out} "${dir}" PARENT_SCOPE)
            return()
        endif()
    endif()

    string(RANDOM LENGTH 10 rand_str)
    set(tmp_str "tmp.${rand_str}")

    # create something in /tmp if it exists
    if(EXISTS "/tmp" AND IS_DIRECTORY "/tmp")
        set(dir "/tmp/${tmp_str}")
        file(MAKE_DIRECTORY "${dir}" RESULT res)
        if (res EQUAL 0)
            set(${out} "${dir}" PARENT_SCOPE)
            return()
        endif()
    endif()

    # tmpdir does not exist, extremely legacy mode
    set(dir "${CMAKE_CURRENT_LIST_DIR}/.tmp/${tmp_str}")
    file(MAKE_DIRECTORY "${dir}" RESULT res)
    if (res EQUAL 0)
        set(${out} "${dir}" PARENT_SCOPE)
        return()
    endif()

    fatal("Fatal: Could not create temporary directory. "
        "Check write permissions to the current directory")
endfunction()

# Get a package's effective URL, for an already parsed object
function(get_package_url_object out)
    if (${url})
        set(${out} "${url}")
    else()
        get_package_url(URL_OUT "${out}"
            GIT_HOST "${git_host}"
            REPO "${repo}"
            VERSION "${version}"
            ARTIFACT "${artifact}"
            PACKAGE "${package}")
    endif()

    return(PROPAGATE ${out})
endfunction()

# Fetch a package from an already-parsed object.
function(fetch_package_object)
    set(optionArgs FORCE)
    cmake_parse_arguments(ARG "${optionArgs}" "" "" ${ARGN})

    if (${url})
        set(pkg_url "${url}")
    else()
        get_package_url(URL_OUT pkg_url
            GIT_HOST "${git_host}"
            REPO "${repo}"
            VERSION "${version}"
            ARTIFACT "${artifact}"
            PACKAGE "${package}")
    endif()

    if (DEFINED CACHE_PATH_OVERRIDE)
        set(cache_path ${CACHE_PATH_OVERRIDE})
    else()
        get_cache_path(${package} ${version} cache_path)
    endif()

    set(fetch_args
        URL "${pkg_url}"
        HASH "${hash}"
        PATH "${cache_path}"
        PATCHES ${patches})

    if(ARG_FORCE)
        list(APPEND fetch_args FORCE)
    endif()

    fetch_package(${fetch_args})
endfunction()

# Format the cpmfile. Requires one of: jq, python, perl
# If you don't have any of those, sorry not sorry.
# Maybe I should make a shell-based alternative.
function(format_cpmfile)
    # jq is the preferred formatter since it's the fastest
    cpm_find_program(JQ_EXECUTABLE jq)
    if (JQ_EXECUTABLE)
        set(command ${JQ_EXECUTABLE} --indent 4 -S .)
    else()
        # Python is simple and works
        find_package(Python 3.5 COMPONENTS Interpreter QUIET)
        if (Python_FOUND)
            set(command ${Python_EXECUTABLE} -m json.tool
                --indent 4 --sort-keys)
        else()
            # json_pp (part of perl) also works well
            cpm_find_program(JSONPP_EXECUTABLE json_pp)
            if (JSONPP_EXECUTABLE)
                set(json_opts "indent" "indent_length=4" "canonical"
                    "space_after=1" "space_before=0")
                string(JOIN "," json_opts_str ${json_opts})

                set(command ${JSONPP_EXECUTABLE} -f json -t json -json_opt
                    "${json_opts_str}")
            else()
                fatal("Fatal: could not find one of jq, Python, or perl"
                    "(json_pp). Install one of these packages to use"
                    "CPMUtil's tooling. If they ARE installed, your"
                    "CMake installation is broken.")
            endif()
        endif()
    endif()

    get_cpmfile_path(file)
    mktempdir(TMP)
    set(tmp_file ${TMP}/cpmfile.json)

    execute_process(COMMAND ${command}
        INPUT_FILE ${file}
        OUTPUT_FILE ${tmp_file})

    # TODO: error handling, mv, cp?
    file(COPY_FILE ${tmp_file} ${file})

    file(REMOVE_RECURSE ${TMP})
endfunction()

# Computes expected SHA512 hash of a package
function(get_package_hash url out)
    mktempdir(TMP)

    get_filename_component(filename ${url} NAME)
    set(file ${TMP}/${filename})

    cpm_download("${url}" "${file}")
    file(SHA512 ${file} ${out})

    file(REMOVE_RECURSE ${TMP})
    return(PROPAGATE ${out})
endfunction()

# Download and put the content into a variable.
function(cpm_download_var url out)
    mktempdir(TMP)

    set(file ${TMP}/tmp)
    cpm_download("${url}" "${file}")
    file(READ ${file} ${out})

    file(REMOVE_RECURSE ${TMP})
    return(PROPAGATE ${out})
endfunction()

# Check if a URL request succeeds without actually saving anything
function(cpm_url_exists url out)
    foreach(i RANGE 5)
        file(DOWNLOAD "${url}" STATUS ret LOG log TIMEOUT 10)
        list(GET ret 0 code)

        if (code EQUAL 0)
            set(${out} TRUE)
            break()
        else()
            if (log MATCHES "HTTP/[0-9.]+ (429|403)")
                sleep(5)
                continue()
            endif()

            set(${out} FALSE)
            break()
        endif()
    endforeach()

    return(PROPAGATE ${out})
endfunction()

# Get latest tag for a package.
# Requires an already-parsed object
function(get_latest_tag out)
    # TODO: Ci packages
    if (NOT repo OR ci)
        set(${out} null)
        return(PROPAGATE ${out})
    endif()

    # first determine if this is a tag or not
    if (git_host STREQUAL github.com)
        set(api "https://api.github.com/repos/${repo}")
        set(check_endpoint "/git/refs/tags")
    else()
        set(api "https://${git_host}/api/v1/repos/${repo}")
        set(check_endpoint "/tags")
    endif()

    # artifacts must only check releases
    if ("${artifact}" STREQUAL "")
        set(json_key name)
        set(endpoint "/tags")
    else()
        set(json_key tag_name)
        set(endpoint "/releases")
        set(check_endpoint "/releases/tags")
    endif()

    cpm_url_exists("${api}${check_endpoint}/${version}" is_tag)

    if (NOT is_tag)
        set(${out} null)
        return(PROPAGATE ${out})
    endif()

    # strip out prefixes e.g. boost-, openssl-, v
    # NOTE: subrels e.g. -1 at the end may cause issues.
    string(REGEX REPLACE "[^0-9.-]" "" t_numeric_version "${version}")
    string(REGEX REPLACE "-$|^-" "" t_numeric_version "${numeric_version}")

    # now api req
    cpm_download_var("${api}${endpoint}" tags)

    string(JSON len LENGTH ${tags})
    math(EXPR last_index "${len} - 1")

    set(greatest_version_numeric ${t_numeric_version})
    set(greatest_version ${version})

    foreach(i RANGE ${last_index})
        string(JSON tag_obj GET "${tags}" ${i})
        string(JSON tag_name GET "${tag_obj}" ${json_key})

        # same as above
        string(REGEX REPLACE "[^0-9.-]" "" numeric_tag "${tag_name}")
        string(REGEX REPLACE "-$|^-" "" numeric_tag "${numeric_tag}")

        if (numeric_tag VERSION_GREATER_EQUAL greatest_version_numeric)
            set(greatest_version_numeric ${numeric_tag})
            set(greatest_version ${tag_name})
        endif()
    endforeach()

    # numeric version replacement
    if (numeric_version)
        set(${out} ${greatest_version_numeric})
    else()
        set(${out} ${greatest_version})
    endif()

    return(PROPAGATE ${out})
endfunction()

# TODO(crueter): Combine these

# Update hash and version of a package
# Outputs the updated object
function(modify_package object version hash out)
    string(JSON new_object SET "${object}" hash "\"${hash}\"")
    string(JSON new_object SET "${new_object}" version "\"${version}\"")

    set(${out} "${new_object}")
    return(PROPAGATE ${out})
endfunction()

# Update hash and numeric_version of a package
# Outputs the updated object
function(modify_package_numeric object version hash out)
    string(JSON new_object SET "${object}" hash "\"${hash}\"")
    string(JSON new_object SET "${new_object}" numeric_version "\"${version}\"")

    set(${out} "${new_object}")
    return(PROPAGATE ${out})
endfunction()


function(get_cpmfile_keys out)
    get_cpmfile_content(object)

    string(JSON len LENGTH ${object})

    math(EXPR last_index "${len} - 1")
    foreach(i RANGE ${last_index})
        string(JSON key MEMBER ${object} ${i})
        list(APPEND ${out} ${key})
    endforeach()

    return(PROPAGATE ${out})
endfunction()

function(parse_script_args out)
    if(PRINT_USAGE)
        usage()
        cmake_language(EXIT 0)
    endif()

    if(NOT CMAKE_SCRIPT_MODE_FILE OR NOT CMAKE_ARGC)
        set(${out} "" PARENT_SCOPE)
        return()
    endif()

    set(found_script FALSE)
    set(idx 0)

    get_filename_component(script_name "${CMAKE_SCRIPT_MODE_FILE}" NAME)

    while(idx LESS CMAKE_ARGC)
        if(found_script)
            set(arg "${CMAKE_ARGV${idx}}")
            if(NOT arg STREQUAL "--")
                list(APPEND positional_args ${arg})
            endif()
        elseif(CMAKE_ARGV${idx} STREQUAL CMAKE_SCRIPT_MODE_FILE)
            set(found_script TRUE)
        else()
            get_filename_component(arg_name "${CMAKE_ARGV${idx}}" NAME)
            if(arg_name STREQUAL script_name)
                set(found_script TRUE)
            endif()
        endif()
        math(EXPR idx "${idx} + 1")
    endwhile()

    if(ALL_PACKAGES AND NOT NO_ALL)
        get_cpmfile_keys(all_keys)
        if(NO_CI)
            set(filtered_keys)
            foreach(key ${all_keys})
                parse_key(${key})
                if(NOT ci)
                    list(APPEND filtered_keys ${key})
                endif()
            endforeach()
            set(positional_args ${filtered_keys})
        else()
            set(positional_args ${all_keys})
        endif()
    endif()

    set(${out} ${positional_args} PARENT_SCOPE)
endfunction()

# Convert a CMake list to a JSON array
function(list_to_array list out)
    set(${out} "[]")
    set(idx 0)
    foreach(elem ${list})
        string(JSON ${out} SET ${${out}} "${idx}" "\"${elem}\"")
        math(EXPR idx "${idx} + 1")
    endforeach()

    return(PROPAGATE ${out})
endfunction()