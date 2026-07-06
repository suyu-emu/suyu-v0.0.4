# SPDX-FileCopyrightText: Copyright 2026 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

cmake_minimum_required(VERSION 3.31)

if(MSVC OR ANDROID OR IOS)
    set(BUNDLED_DEFAULT ON)
else()
    set(BUNDLED_DEFAULT OFF)
endif()

# TODO: Handle nested cpmutil includes

# Root directory detection
if (CMAKE_SCRIPT_MODE_FILE)
    # In script mode, default to PWD
    # Or you can specify one yourself
    if (NOT DEFINED CPMUTIL_ROOT)
        set(CPMUTIL_ROOT ${CMAKE_SOURCE_DIR})
    endif()
else()
    # A project exists, we can use its directory
    set(CPMUTIL_ROOT ${PROJECT_SOURCE_DIR})
endif()

set(CPM_SOURCE_CACHE "${CPMUTIL_ROOT}/.cache/cpm"
    CACHE PATH "Directory to download CPM dependencies")

option(CPMUTIL_FORCE_BUNDLED
    "Force bundled packages for all CPM depdendencies" ${BUNDLED_DEFAULT})

option(CPMUTIL_FORCE_SYSTEM
    "Force system packages for all CPM dependencies" OFF)

set(CPMUTIL_PATCH_DIR "${CPMUTIL_ROOT}/.patch" CACHE STRING
    "Directory containing patches for packages")

include(CPM)

# Rudimentary target architecture detection
# This is in script mode, CI packages will be nonfunctional. TODO
if (NOT DEFINED CMAKE_SYSTEM_PROCESSOR)
    message(DEBUG "[CPMUtil] No system architecture found, "
        "CI packages are nonfunctional")
elseif (NOT DEFINED ARCHITECTURE)
    string(TOLOWER ${CMAKE_SYSTEM_PROCESSOR} processor)
    if (processor MATCHES "x86|amd64")
        set(CPMUTIL_AMD64 ON)
    elseif(processor MATCHES "^aarch64|^arm64|^armv8\.*")
        set(CPMUTIL_ARM64 ON)
    elseif(processor MATCHES "riscv")
        set(CPMUTIL_RISCV64 ON)
    endif()
else()
    # This block exists for compatibility with my own DetectArchitecture.cmake.
    if (ARCHITECTURE_x86_64)
        set(CPMUTIL_AMD64 ON)
    elseif(ARCHITECTURE_arm64)
        set(CPMUTIL_ARM64 ON)
    elseif(ARCHITECTURE_riscv64)
        set(CPMUTIL_RISCV64 ON)
    endif()
endif()

# cpmfile parsing
set(CPMUTIL_JSON_FILE "${CPMUTIL_ROOT}/cpmfile.json")

if(EXISTS ${CPMUTIL_JSON_FILE})
    file(READ ${CPMUTIL_JSON_FILE} CPMFILE_CONTENT)

    # Script mode doesn't support add_custom_target etc.
    if (NOT CMAKE_SCRIPT_MODE_FILE)
        if (NOT TARGET cpmfiles)
            add_custom_target(cpmfiles)
        endif()

        target_sources(cpmfiles PRIVATE ${CPMUTIL_JSON_FILE})
        set_property(DIRECTORY APPEND PROPERTY
            CMAKE_CONFIGURE_DEPENDS
            "${CPMUTIL_JSON_FILE}")
    endif()
else()
    message(DEBUG "[CPMUtil] cpmfile ${CPMUTIL_JSON_FILE} "
        "does not exist, AddJsonPackage will be a no-op")
endif()

#############
# UTILITIES #
#############

# Analogous to POSIX echo
macro(echo)
    string(REPLACE ";" " " message "${ARGN}")
    if (CMAKE_SCRIPT_MODE_FILE)
        execute_process(COMMAND ${CMAKE_COMMAND} -E echo
            "${message}")
    else()
        message(STATUS "${message}")
    endif()
endmacro()

# Analogous to POSIX echo, but to stderr
macro(echo_error)
    string(REPLACE ";" " " message "${ARGN}")
    message(NOTICE "${message}")
endmacro()

# Fatal error, accounting for script mode
macro(fatal)
    if (CMAKE_SCRIPT_MODE_FILE)
        echo_error(${ARGN})
        cmake_language(EXIT 1)
    else()
        message(FATAL_ERROR ${ARGN})
    endif()
endmacro()

# Analogous to POSIX sleep
macro(sleep time)
    execute_process(COMMAND ${CMAKE_COMMAND} -E sleep ${time})
endmacro()

# Analogous to GNU mktemp, with fallbacks
function(mktempdir out)
    # shell out to system mktemp if available
    find_program(MKTEMP_EXECUTABLE mktemp)
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

# Get a package's effective URL.
function(get_package_url)
    set(oneValueArgs
        GIT_HOST
        REPO
        VERSION
        ARTIFACT
        PACKAGE

        URL_OUT
        GIT_URL_OUT)

    cmake_parse_arguments(ARG "" "${oneValueArgs}" "" ${ARGN})

    if (NOT DEFINED ARG_URL_OUT)
        fatal("get_package_url: URL_OUT is required")
    endif()

    if(NOT DEFINED ARG_GIT_HOST)
        set(ARG_GIT_HOST github.com)
    endif()

    if(DEFINED ARG_REPO)
        set(url https://${ARG_GIT_HOST}/${ARG_REPO})

        if (DEFINED ARG_GIT_URL_OUT)
            set(${ARG_GIT_URL_OUT} "${url}")
        endif()

        if(DEFINED ARG_ARTIFACT)
            set(url
                "${url}/releases/download/${ARG_VERSION}/${ARG_ARTIFACT}")
        else()
            set(url "${url}/archive/${ARG_VERSION}.tar.gz")
        endif()
    else()
        fatal("${package}: No URL or repository defined")
    endif()

    set(${ARG_URL_OUT} "${url}")

    return(PROPAGATE ${ARG_URL_OUT} ${ARG_GIT_URL_OUT})
endfunction()

# Get a package's cache path.
function(get_cache_path package key out)
    string(TOLOWER ${package} lower_name)

    # TODO: Figure out a sln to CPM_SOURCE_CACHE; CPMConfig.cmake?
    set(${out} ${CPM_SOURCE_CACHE}/${lower_name}/${key})

    return(PROPAGATE ${out})
endfunction()

# Download a URL to file, with a sha512 hash
# And retry 5 times
function(cpm_download url file)
    list(LENGTH ARGN argn_len)
    if(argn_len GREATER 0)
        list(GET ARGN 0 hash)
        set(args EXPECTED_HASH SHA512=${hash})
    else()
        unset(args)
    endif()

    foreach(i RANGE 5)
        file(DOWNLOAD ${url} ${file}
            ${args}
            STATUS ret
            LOG log)

        list(GET ret 0 code)
        if (code EQUAL 0)
            break()
        endif()

        echo_error("Download failed. Trying again in 5 seconds")
        sleep(5)
    endforeach()

    # TODO: use return code or something
    if (NOT code EQUAL 0)
        fatal("Download for ${pkg_url} failed after 5 tries")
    endif()
endfunction()

# Wrapper around find_program that works with Git for Windows
macro(cpm_find_program)
    # Windows needs additional paths for some utilities.
    if (CMAKE_HOST_WIN32)
        find_package(Git QUIET)
        if(Git_FOUND)
            # Search within the Git for Windows paths.
            get_filename_component(extra_search_path
                ${GIT_EXECUTABLE} DIRECTORY)
            get_filename_component(extra_search_path_1up
                ${extra_search_path} DIRECTORY)
            get_filename_component(extra_search_path_2up
                ${extra_search_path_1up} DIRECTORY)

            set(base_hints "${extra_search_path_1up}/usr/bin"
                "${extra_search_path_2up}/usr/bin")

            # Also add core_perl to the paths, for perl commands like json_pp
            set(hints "")
            foreach(hint ${base_hints})
                list(APPEND hints
                    "${hint}"
                    "${hint}/core_perl")
            endforeach()

            find_program(${ARGN} HINTS ${hints})
        endif()

        # If no Git is found, continue as normal
    endif()

    find_program(${ARGN})
endmacro()

# Apply patches to a directory.
function(apply_patches patches dir)
    cpm_find_program(PATCH_EXE patch)
    if (NOT PATCH_EXE)
        fatal("Could not find patch executable")
    endif()

    foreach(patch ${patches})
        get_filename_component(patch_name ${patch} NAME)
        echo("-- Applying patch ${patch_name}")
        execute_process(
            COMMAND ${PATCH_EXE} -p1
            INPUT_FILE ${patch}
            WORKING_DIRECTORY ${dir})
    endforeach()
endfunction()

# Fetches a file to the CPM source cache.
function(fetch_package)
    set(oneValueArgs
        URL
        HASH
        PATH

        PATCH_KEY)

    set(multiValueArgs PATCHES)

    set(optionArgs FORCE)

    cmake_parse_arguments(ARG
        "${optionArgs}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if (NOT DEFINED ARG_URL)
        fatal("fetch_package: URL is required")
    endif()

    if (NOT DEFINED ARG_HASH)
        fatal("fetch_package: HASH is required")
    endif()

    if (NOT DEFINED ARG_PATH)
        fatal("fetch_package: PATH is required")
    endif()

    if (NOT DEFINED ARG_PATCH_KEY)
        compute_patch_key("${ARG_PATCHES}" ARG_PATCH_KEY)
    endif()

    # refetch if cache is invalid or forced
    needs_refetch(${ARG_PATH} "${ARG_PATCH_KEY}" CACHE_INVALID)

    if (ARG_FORCE OR CACHE_INVALID)
        file(REMOVE_RECURSE ${ARG_PATH})
    else()
        return()
    endif()

    # Temporary directory.
    mktempdir(TMP)

    # Get filename from URL
    get_filename_component(base_filename ${ARG_URL} NAME)

    # Download
    set(file ${TMP}/${base_filename})
    cpm_download(${ARG_URL} ${file} ${ARG_HASH})
    message(DEBUG "Downloaded ${base_filename}")

    # Extract the downloaded archive
    # TODO: Moar error handling
    set(dir ${TMP}/${base_filename}-extracted)
    file(MAKE_DIRECTORY ${dir})

    file(ARCHIVE_EXTRACT
        INPUT ${file}
        DESTINATION ${dir})

    # This is copied near-verbatim from ExternalProject/extractfile.cmake.in

    # If there's just one subdirectory and nothing else, move it
    file(GLOB contents "${dir}/*")
    list(REMOVE_ITEM contents "${dir}/.DS_Store")
    list(LENGTH contents n)

    # If n == 1 and contents points to a directory, this is a GitHub-style pack
    # In this case contents points to the subdir which will get renamed
    # If not, contents will point to the parent dir which will get renamed
    if (NOT n EQUAL 1 OR NOT IS_DIRECTORY "${contents}")
        set(contents "${dir}")
    endif()

    file(REAL_PATH "${contents}" contents_abs)

    # paths
    cmake_path(ABSOLUTE_PATH ARG_PATH
        NORMALIZE
        OUTPUT_VARIABLE abs_path)

    cmake_path(GET abs_path PARENT_PATH path_parent)
    cmake_path(GET abs_path FILENAME path_name)

    # rename tmp dir
    set(tmp_renamed "${TMP}/${path_name}")
    file(RENAME "${contents_abs}" "${tmp_renamed}")

    # now copy
    # TODO: Error handling beyond what cmake does????
    file(COPY ${tmp_renamed} DESTINATION ${path_parent})

    # TODO: only echo this in script mode
    message(DEBUG "Extracted to ${abs_path}")

    # Apply patches
    apply_patches("${ARG_PATCHES}" "${abs_path}")

    # Write patch key
    file(WRITE "${abs_path}/.cpm_patch_key" ${ARG_PATCH_KEY})

    # done! :)
    file(REMOVE_RECURSE ${TMP})
endfunction()

# compute a hash of all patch file contents
# if there are no patches, returns none
function(compute_patch_key patches out)
  if(NOT patches)
    set("${out}" "none" PARENT_SCOPE)
    return()
  endif()

  set(combined "")
  foreach(PATCH ${patches})
    file(READ "${PATCH}" contents)
    string(APPEND combined "${contents}")
  endforeach()

  string(SHA512 key "${combined}")
  set("${out}" "${key}" PARENT_SCOPE)
endfunction()

# Check if a package needs to be redownloaded. This can occur if:
# - Patch key is missing or mismatched
# - Package fetch dir is missing
# path is the package cache path
function(needs_refetch path patch_key out)
    set(patch_key_file "${path}/.cpm_patch_key")

    # download directory is empty or patch key file is missing
    if (NOT EXISTS ${patch_key_file})
        set(${out} TRUE PARENT_SCOPE)
        return()
    endif()

    # compare patch key
    file(READ "${patch_key_file}" current_patch_key)
    if (NOT current_patch_key STREQUAL patch_key)
        set(${out} TRUE PARENT_SCOPE)
    else()
        set(${out} FALSE PARENT_SCOPE)
    endif()
endfunction()

# Message
function(cpm_utils_message level)
    string(REPLACE ";" " " message "${ARGN}")
    message(${level} "[CPMUtil] ${message}")
endfunction()

# propagate a variable to parent scope
macro(Propagate var)
    set(${var} ${${var}} PARENT_SCOPE)
endmacro()

# Get cpmfile content
function(get_cpmfile_content out)
    set(${out} "${CPMFILE_CONTENT}" PARENT_SCOPE)
endfunction()

# Get absolute path to cpmfile
function(get_cpmfile_path out)
    file(REAL_PATH ${CPMUTIL_JSON_FILE} ${out})
    return(PROPAGATE ${out})
endfunction()

# idk
function(array_to_list array length out)
    math(EXPR range "${length} - 1")

    foreach(IDX RANGE ${range})
        string(JSON _element GET "${array}" "${IDX}")

        list(APPEND NEW_LIST ${_element})
    endforeach()

    set("${out}" "${NEW_LIST}" PARENT_SCOPE)
endfunction()

# json
function(get_json_element object out member)
    list(LENGTH ARGN argn_len)
    if(argn_len GREATER 0)
        list(GET ARGN 0 default)
    endif()

    string(JSON out_type ERROR_VARIABLE err TYPE "${object}" ${member})

    if(err)
        if (DEFINED default)
            set("${out}" "${default}" PARENT_SCOPE)
        else()
            unset("${out}" PARENT_SCOPE)
        endif()

        return()
    endif()

    string(JSON outvar GET "${object}" ${member})

    if(out_type STREQUAL "ARRAY")
        string(JSON _len LENGTH "${object}" ${member})
        set("${out}_LENGTH" "${_len}" PARENT_SCOPE)
    endif()

    set("${out}" "${outvar}" PARENT_SCOPE)
endfunction()

# register a package to CPMUtil's global registry
function(cpmutil_register_package name url version)
    set_property(GLOBAL APPEND PROPERTY CPM_PACKAGE_NAMES ${name})
    set_property(GLOBAL APPEND PROPERTY CPM_PACKAGE_URLS ${url})
    set_property(GLOBAL APPEND PROPERTY CPM_PACKAGE_SHAS "${version}")
endfunction()

# Determine whether or not a package has a viable system candidate.
function(SystemPackageViable JSON_NAME)
    string(JSON object GET "${CPMFILE_CONTENT}" "${JSON_NAME}")

    parse_object(${object})

    string(REPLACE " " ";" find_args "${find_args}")
    if(${package}_FORCE_BUNDLED OR
        (CPMUTIL_FORCE_BUNDLED
         AND NOT ${package}_FORCE_SYSTEM
         AND NOT CPMUTIL_FORCE_SYSTEM))
        set(${package}_FOUND OFF)
    else()
        find_package(${package} ${min_version} ${find_args} QUIET NO_POLICY_SCOPE)
    endif()

    set(${JSON_NAME}_VIABLE ${${package}_FOUND} PARENT_SCOPE)
    set(${JSON_NAME}_PACKAGE ${package} PARENT_SCOPE)
endfunction()

# Add several packages such that if one is bundled,
# all the rest must also be bundled.
function(AddDependentPackages)
    set(_some_system OFF)
    set(_some_bundled OFF)

    foreach(pkg ${ARGN})
        SystemPackageViable(${pkg})

        if (${pkg}_VIABLE)
            set(_some_system ON)
            list(APPEND _system_pkgs ${${pkg}_PACKAGE})
        else()
            set(_some_bundled ON)
            list(APPEND _bundled_pkgs ${${pkg}_PACKAGE})
        endif()
    endforeach()

    if (_some_system AND _some_bundled)
        foreach(pkg ${ARGN})
            list(APPEND package_names ${${pkg}_PACKAGE})
        endforeach()

        string(REPLACE ";" ", " package_names "${package_names}")
        string(REPLACE ";" ", " bundled_names "${_bundled_pkgs}")
        foreach(sys ${_system_pkgs})
            list(APPEND system_names ${sys}_FORCE_BUNDLED)
        endforeach()

        string(REPLACE ";" ", " system_names "${system_names}")

        message(FATAL_ERROR "Partial dependency installation detected "
            "for the following packages:\n${package_names}\n"
            "You can solve this in one of two ways:\n"
            "1. Install or upgrade the following packages "
            "to your system if available:"
            "\n\t${bundled_names}\n"
            "2. Set the following variables to ON:"
            "\n\t${system_names}\n"
            "This may also be caused by a version mismatch, "
            "such as one package being newer than the other.")
    endif()

    foreach(pkg ${ARGN})
        AddJsonPackage(${pkg})
    endforeach()
endfunction()

# process version/artifact replacements
macro(process_version_replacements)
    # numeric version text replacement
    if (numeric_version)
        string(REPLACE "%NUMERIC_VERSION%" "${numeric_version}"
            version ${version})
        string(REPLACE "%NUMERIC_VERSION%" "${numeric_version}"
            artifact ${artifact})
    endif()

    # version text replacement
    if (artifact)
        string(REPLACE "%VERSION%" "${version}"
            artifact ${artifact})
    endif()
endmacro()

# json util
macro(parse_object object)
    get_json_element("${object}" package package ${JSON_NAME})
    get_json_element("${object}" repo repo)
    get_json_element("${object}" ci ci OFF)
    get_json_element("${object}" version version)
    get_json_element("${object}" min_version min_version)
    get_json_element("${object}" git_host git_host "github.com")

    if (NOT version)
        cpm_utils_message(FATAL_ERROR "${JSON_NAME}: version is required")
    endif()

    if(ci)
        get_json_element("${object}" name name "${JSON_NAME}")
        get_json_element("${object}" extension extension "tar.zst")
        get_json_element("${object}" raw_disabled disabled_platforms)

        if(raw_disabled)
            array_to_list("${raw_disabled}"
                ${raw_disabled_LENGTH} disabled_platforms)
        else()
            set(disabled_platforms "")
        endif()
    else()
        # TODO: correct hash if missing
        get_json_element("${object}" hash hash)
        get_json_element("${object}" url url)
        get_json_element("${object}" artifact artifact)
        get_json_element("${object}" source_subdir source_subdir)
        get_json_element("${object}" bundled bundled "unset")
        get_json_element("${object}" numeric_version numeric_version)
        get_json_element("${object}" find_args find_args)
        get_json_element("${object}" skip_updates skip_updates OFF)
        get_json_element("${object}" raw_patches patches)

        process_version_replacements()

        # format patchdir
        unset(patches)
        if(raw_patches)
            math(EXPR range "${raw_patches_LENGTH} - 1")

            foreach(IDX RANGE ${range})
                string(JSON _patch GET "${raw_patches}" "${IDX}")

                set(full_patch
                    "${CPMUTIL_PATCH_DIR}/${JSON_NAME}/${_patch}")
                if(NOT EXISTS ${full_patch})
                    cpm_utils_message(FATAL_ERROR
                        "${JSON_NAME} specifies patch"
                        "${full_patch} which does not exist")
                endif()

                list(APPEND patches "${full_patch}")
            endforeach()
        endif()
        # end format patchdir

        # options
        get_json_element("${object}" raw_options options)

        unset(options)
        if(raw_options)
            array_to_list("${raw_options}" ${raw_options_LENGTH} options)
        endif()

        set(options ${options} ${JSON_OPTIONS})
        # end options

        # system/bundled
        if(bundled STREQUAL "unset" AND DEFINED JSON_BUNDLED_PACKAGE)
            set(bundled ${JSON_BUNDLED_PACKAGE})
        endif()
    endif()
endmacro()

# Get the JSON object for a key. Outputs to "object"
function(get_json_object key)
    if(NOT DEFINED CPMFILE_CONTENT)
        cpm_utils_message(FATAL_ERROR "${key}: No cpmfile present")
    endif()

    if(NOT DEFINED key)
        cpm_utils_message(FATAL_ERROR "No JSON key specified")
    endif()

    string(JSON object ERROR_VARIABLE
        err GET "${CPMFILE_CONTENT}" "${key}")

    if(err)
        cpm_utils_message(FATAL_ERROR "${key} not found in cpmfile\n"
            "${err}")
    endif()

    Propagate(object)
endfunction()

# Get the download URL given the parameters.

# The preferred usage
function(AddJsonPackage)
    set(oneValueArgs
        NAME

        # these are overrides that can be generated at runtime,
        # so can be defined separately from the json
        FORCE_BUNDLED_PACKAGE)

    set(multiValueArgs OPTIONS)

    set(optionArgs MODULE_PATH DOWNLOAD_ONLY)

    cmake_parse_arguments(JSON
        "${optionArgs}"
        "${oneValueArgs}"
        "${multiValueArgs}"
        "${ARGN}")

    list(LENGTH ARGN argnLength)

    # single name argument
    if(argnLength EQUAL 1)
        set(JSON_NAME "${ARGV0}")
    endif()

    get_json_object("${JSON_NAME}")
    parse_object(${object})

    unset(EXTRA_ARGS)
    if (JSON_MODULE_PATH)
        list(APPEND EXTRA_ARGS MODULE_PATH)
    endif()

    if (JSON_DOWNLOAD_ONLY)
        list(APPEND EXTRA_ARGS DOWNLOAD_ONLY)
    endif()

    if(ci)
        AddCIPackage(
            VERSION "${version}"
            NAME "${name}"
            REPO "${repo}"
            PACKAGE "${package}"
            EXTENSION "${extension}"
            DISABLED_PLATFORMS "${disabled_platforms}"

            GIT_HOST "${git_host}"

            ${EXTRA_ARGS})
    else()
        if (NOT DEFINED JSON_FORCE_BUNDLED_PACKAGE)
            set(JSON_FORCE_BUNDLED_PACKAGE OFF)
        endif()

        AddPackage(
            NAME "${package}"
            VERSION "${version}"
            MIN_VERSION "${min_version}"
            URL "${url}"
            HASH "${hash}"
            REPO "${repo}"
            PATCHES "${patches}"
            OPTIONS "${options}"
            FIND_PACKAGE_ARGUMENTS "${find_args}"
            BUNDLED_PACKAGE "${bundled}"
            FORCE_BUNDLED_PACKAGE "${JSON_FORCE_BUNDLED_PACKAGE}"
            SOURCE_SUBDIR "${source_subdir}"

            GIT_HOST "${git_host}"

            ARTIFACT "${artifact}"

            ${EXTRA_ARGS})
    endif()

    # pass stuff to parent scope
    Propagate(${package}_ADDED)
    Propagate(${package}_SOURCE_DIR)
    Propagate(${package}_BINARY_DIR)
    Propagate(CMAKE_PREFIX_PATH)
endfunction()

# Internal function
function(AddPackage)
    set(EXTRA_ARGS "")

    set(oneValueArgs
        NAME
        MIN_VERSION
        GIT_HOST

        REPO
        VERSION
        ARTIFACT

        HASH

        URL
        CUSTOM_KEY

        SOURCE_SUBDIR
        BUNDLED_PACKAGE
        FORCE_BUNDLED_PACKAGE
        FIND_PACKAGE_ARGUMENTS)

    set(multiValueArgs OPTIONS PATCHES)

    set(optionArgs MODULE_PATH DOWNLOAD_ONLY)

    cmake_parse_arguments(PKG_ARGS
        "${optionArgs}"
        "${oneValueArgs}"
        "${multiValueArgs}"
        "${ARGN}")

    if(NOT DEFINED PKG_ARGS_NAME)
        cpm_utils_message(FATAL_ERROR "AddPackage: NAME is required")
    endif()

    if(NOT DEFINED PKG_ARGS_VERSION)
        cpm_utils_message(FATAL_ERROR "${PKG_ARGS_NAME}: VERSION is required")
    endif()

    set(${PKG_ARGS_NAME}_CUSTOM_DIR "" CACHE STRING
        "Path to a separately-downloaded copy of ${PKG_ARGS_NAME}")
    option(${PKG_ARGS_NAME}_FORCE_SYSTEM
        "Force the system package for ${PKG_ARGS_NAME}")
    option(${PKG_ARGS_NAME}_FORCE_BUNDLED
        "Force the bundled package for ${PKG_ARGS_NAME}")

    # URL parsing
    if (NOT DEFINED PKG_ARGS_URL)
        get_package_url(URL_OUT "PKG_ARGS_URL"
            GIT_URL_OUT "pkg_git_url"
            GIT_HOST "${PKG_ARGS_GIT_HOST}"
            REPO "${PKG_ARGS_REPO}"
            VERSION "${PKG_ARGS_VERSION}"
            ARTIFACT "${PKG_ARGS_ARTIFACT}"
            PACKAGE "${PKG_ARGS_NAME}")
    else()
        set(pkg_git_url ${PKG_ARGS_URL})
    endif()

    cpm_utils_message(DEBUG "${PKG_ARGS_NAME} download URL is ${PKG_ARGS_URL}")

    if(NOT DEFINED PKG_ARGS_HASH)
        cpm_utils_message(FATAL_ERROR "${PKG_ARGS_NAME}: No hash defined")
    endif()

    #[[
        Precedence:
        - FORCE_BUNDLED_PACKAGE (caller override)
        - package_FORCE_SYSTEM, package_FORCE_BUNDLED (user option)
        - CPMUTIL_FORCE_SYSTEM, CPMUTIL_FORCE_BUNDLED (global option)
        - BUNDLED_PACKAGE (json default)
        - default to allow system
    ]]
    if(PKG_ARGS_FORCE_BUNDLED_PACKAGE)
        set(use_system OFF)
    elseif(${PKG_ARGS_NAME}_FORCE_SYSTEM)
        set(use_system ON)
        set(force_system ON)
    elseif(${PKG_ARGS_NAME}_FORCE_BUNDLED)
        set(use_system OFF)
    elseif(CPMUTIL_FORCE_SYSTEM)
        set(use_system ON)
        set(force_system ON)
    elseif(CPMUTIL_FORCE_BUNDLED)
        set(use_system OFF)
    elseif(DEFINED PKG_ARGS_BUNDLED_PACKAGE AND
        NOT PKG_ARGS_BUNDLED_PACKAGE STREQUAL "unset")
        if(PKG_ARGS_BUNDLED_PACKAGE)
            set(use_system OFF)
        else()
            set(use_system ON)
        endif()
    else()
        set(use_system ON)
    endif()

    # find_package nonsense
    if(use_system)
        set(find_args ${PKG_ARGS_NAME})
        if(DEFINED PKG_ARGS_MIN_VERSION)
            list(APPEND find_args ${PKG_ARGS_MIN_VERSION})
        endif()

        if (DEFINED PKG_ARGS_FIND_PACKAGE_ARGUMENTS)
            string(REPLACE " " ";"
                passed_find_args
                "${PKG_ARGS_FIND_PACKAGE_ARGUMENTS}")

            set(find_args "${find_args};${passed_find_args}")
        endif()

        find_package(${find_args} QUIET)

        if(${PKG_ARGS_NAME}_FOUND)
            if(DEFINED ${PKG_ARGS_NAME}_VERSION)
                set(sys_ver ${${PKG_ARGS_NAME}_VERSION})
            else()
                set(sys_ver ${PKG_ARGS_VERSION})
            endif()

            message(STATUS
                "[CPMUtil] Using system package ${PKG_ARGS_NAME}@${sys_ver}")

            CPMRegisterPackage(${PKG_ARGS_NAME} "${sys_ver}")

            cpmutil_register_package(
                ${PKG_ARGS_NAME}
                ${pkg_git_url}
                "${sys_ver} (system)")

            set(${PKG_ARGS_NAME}_ADDED NO PARENT_SCOPE)

            return()
        endif()

        if(force_system)
            string(REPLACE ";" " " str_find_args "${find_args}")
            message(FATAL_ERROR "[CPMUtil] ${PKG_ARGS_NAME} not found via "
                "find_package(${str_find_args})")
        endif()
    endif()

    # extra options
    if (PKG_ARGS_OPTIONS)
        list(APPEND EXTRA_ARGS OPTIONS "${PKG_ARGS_OPTIONS}")
    endif()

    if (PKG_ARGS_SOURCE_SUBDIR)
        list(APPEND EXTRA_ARGS SOURCE_SUBDIR "${PKG_ARGS_SOURCE_SUBDIR}")
    endif()

    if (PKG_ARGS_DOWNLOAD_ONLY OR PKG_ARGS_MODULE_PATH)
        list(APPEND EXTRA_ARGS DOWNLOAD_ONLY ON)
    endif()

    cpm_utils_message(STATUS
        "Using bundled package ${PKG_ARGS_NAME}@${PKG_ARGS_VERSION}")

    # Download/extract package
    if (${PKG_ARGS_NAME}_CUSTOM_DIR STREQUAL "")
        if (DEFINED PKG_ARGS_CUSTOM_KEY)
            set(key ${PKG_ARGS_CUSTOM_KEY})
        else()
            set(key ${PKG_ARGS_VERSION})
        endif()

        # cache path
        get_cache_path(${PKG_ARGS_NAME} ${key} cache_path)

        fetch_package(
            URL "${PKG_ARGS_URL}"
            HASH "${PKG_ARGS_HASH}"
            PATH "${cache_path}"
            PATCHES "${PKG_ARGS_PATCHES}")

        set(source_dir "${cache_path}")
    else()
        set(source_dir "${${PKG_ARGS_NAME}_CUSTOM_DIR}")
    endif()

    CPMAddPackage(
        NAME ${PKG_ARGS_NAME}
        VERSION ${PKG_ARGS_VERSION}
        SOURCE_DIR ${source_dir}

        EXCLUDE_FROM_ALL ON

        ${EXTRA_ARGS}
        ${PKG_ARGS_UNPARSED_ARGUMENTS})

    cpmutil_register_package(
        ${PKG_ARGS_NAME}
        ${pkg_git_url}
        ${PKG_ARGS_VERSION})

    set(${PKG_ARGS_NAME}_ADDED YES PARENT_SCOPE)
    Propagate(${PKG_ARGS_NAME}_SOURCE_DIR)
    Propagate(${PKG_ARGS_NAME}_BINARY_DIR)

    if (PKG_ARGS_MODULE_PATH)
        list(PREPEND CMAKE_PREFIX_PATH "${${ARTIFACT_PACKAGE}_SOURCE_DIR}")
        Propagate(CMAKE_PREFIX_PATH)
    endif()
endfunction()

# TODO(crueter): we could do an AddMultiArchPackage, multiplatformpackage?
# name is the artifact name, package is for find_package override
function(AddCIPackage)
    set(oneValueArgs
        VERSION
        NAME
        REPO
        PACKAGE
        EXTENSION
        MIN_VERSION
        GIT_HOST)

    set(multiValueArgs DISABLED_PLATFORMS)

    set(optionArgs MODULE_PATH)

    cmake_parse_arguments(PKG_ARGS
        "${optionArgs}"
        "${oneValueArgs}"
        "${multiValueArgs}"
        ${ARGN})

    if(NOT DEFINED PKG_ARGS_VERSION)
        cpm_utils_message(FATAL_ERROR "VERSION is required")
    endif()

    if(NOT DEFINED PKG_ARGS_NAME)
        cpm_utils_message(FATAL_ERROR "NAME is required")
    endif()

    if(NOT DEFINED PKG_ARGS_REPO)
        cpm_utils_message(FATAL_ERROR "REPO is required")
    endif()

    if(NOT DEFINED PKG_ARGS_PACKAGE)
        cpm_utils_message(FATAL_ERROR "PACKAGE is required")
    endif()

    if(NOT DEFINED PKG_ARGS_EXTENSION)
        set(ARTIFACT_EXT "tar.zst")
    else()
        set(ARTIFACT_EXT ${PKG_ARGS_EXTENSION})
    endif()

    if (NOT DEFINED PKG_ARGS_GIT_HOST)
        set(ARTIFACT_GIT_HOST "github.com")
    else()
        set(ARTIFACT_GIT_HOST "${PKG_ARGS_GIT_HOST}")
    endif()

    if(DEFINED PKG_ARGS_DISABLED_PLATFORMS)
        set(DISABLED_PLATFORMS ${PKG_ARGS_DISABLED_PLATFORMS})
    endif()

    # this is mildly annoying
    set(ARTIFACT_VERSION ${PKG_ARGS_VERSION})
    set(ARTIFACT_NAME ${PKG_ARGS_NAME})
    set(ARTIFACT_REPO ${PKG_ARGS_REPO})
    set(ARTIFACT_PACKAGE ${PKG_ARGS_PACKAGE})

    # TODO: Use amd64/aarch64 naming for everything.
    # Also drop macos universal

    if (MSVC)
        set(platname windows)
    elseif(MINGW)
        set(platname mingw)
    elseif(ANDROID)
        set(platname android)
    elseif(LINUX)
        set(platname linux)
    elseif(IOS)
        set(platname ios)
    elseif(APPLE)
        set(platname macos)
    else()
        cpm_utils_message(WARNING
            "Unsupported platform ${CMAKE_SYSTEM_NAME} for CI packages")
    endif()

    if (APPLE AND NOT IOS)
        set(archname universal)
    elseif((WIN32 OR LINUX) AND CPMUTIL_AMD64)
        set(archname amd64)
    elseif(WIN32 AND CPMUTIL_ARM64)
        set(archname arm64)
    elseif((IOS OR LINUX OR ANDROID) AND CPMUTIL_ARM64)
        set(archname aarch64)
    elseif(ANDROID AND CPMUTIL_AMD64)
        set(archname x86_64)
    else()
        cpm_utils_message(WARNING
            "Unsupported platform/arch combo for CI packages")
    endif()

    if (DEFINED platname AND DEFINED archname)
        set(pkgname ${platname}-${archname})
    endif()

    if (DEFINED pkgname
        AND NOT "${pkgname}" IN_LIST DISABLED_PLATFORMS)
        set(ARTIFACT
            "${ARTIFACT_NAME}-${pkgname}-${ARTIFACT_VERSION}.${ARTIFACT_EXT}")

        if (PKG_ARGS_MODULE_PATH)
            set(EXTRA_ARGS MODULE_PATH)
        endif()

        # download sha512sum file
        # TODO: CI pkgs
        set(sha512sum_url
            "https://${ARTIFACT_GIT_HOST}/${ARTIFACT_REPO}/releases/download/v${ARTIFACT_VERSION}/${ARTIFACT}.sha512sum")
        set(sha512sum_file
            "${CMAKE_CURRENT_BINARY_DIR}/.cpmutil_${ARTIFACT}_sha512sum")

        file(DOWNLOAD "${sha512sum_url}" "${sha512sum_file}"
            STATUS sha512sum_status)
        list(GET sha512sum_status 0 sha512sum_error)

        if(sha512sum_error)
            message(FATAL_ERROR "[CPMUtil] Failed to download sha512sum "
                "for ${ARTIFACT_NAME} from ${sha512sum_url}")
        endif()

        file(READ "${sha512sum_file}" sha512sum_hash)
        string(STRIP "${sha512sum_hash}" sha512sum_hash)
        file(REMOVE "${sha512sum_file}")

        AddPackage(
            NAME ${ARTIFACT_PACKAGE}
            REPO ${ARTIFACT_REPO}
            VERSION "v${ARTIFACT_VERSION}"
            ARTIFACT ${ARTIFACT}
            CUSTOM_KEY "${ARTIFACT_VERSION}-${pkgname}"
            HASH ${sha512sum_hash}
            FORCE_BUNDLED_PACKAGE ON
            ${EXTRA_ARGS})

        set(${ARTIFACT_PACKAGE}_ADDED TRUE PARENT_SCOPE)
        Propagate(${ARTIFACT_PACKAGE}_SOURCE_DIR)
        Propagate(CMAKE_PREFIX_PATH)
    else()
        cpm_utils_message(FATAL_ERROR "${ARTIFACT_NAME}:"
            "Unsupported platform ${pkgname} for CI package")
    endif()
endfunction()

# Utility function for Qt
function(AddQt repo version)
    if (NOT DEFINED repo)
        message(FATAL_ERROR "[CPMUtil] AddQt: repo is required")
    endif()

    if (NOT DEFINED version)
        message(FATAL_ERROR "[CPMUtil] AddQt: version is required")
    endif()

    AddCIPackage(
        NAME qt
        PACKAGE Qt6
        VERSION ${version}
        REPO ${repo}
        DISABLED_PLATFORMS
            android-x86_64 android-aarch64
        MODULE_PATH)

    find_package(Qt6 REQUIRED PATHS ${Qt6_SOURCE_DIR} NO_DEFAULT_PATH)

    Propagate(CMAKE_PREFIX_PATH)
    Propagate(Qt6_SOURCE_DIR)
endfunction()
