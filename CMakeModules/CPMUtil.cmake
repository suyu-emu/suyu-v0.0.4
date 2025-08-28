# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# SPDX-FileCopyrightText: Copyright 2025 crueter
# SPDX-License-Identifier: GPL-3.0-or-later

# Created-By: crueter
# Docs will come at a later date, mostly this is to just reduce boilerplate
# and some cmake magic to allow for runtime viewing of dependency versions

# Future crueter: Wow this was a lie and a half, at this point I might as well make my own CPN
# haha just kidding... unless?

if (MSVC OR ANDROID)
    set(BUNDLED_DEFAULT OFF)
else()
    set(BUNDLED_DEFAULT ON)
endif()

option(CPMUTIL_FORCE_BUNDLED
    "Force bundled packages for all CPM depdendencies" ${BUNDLED_DEFAULT})

option(CPMUTIL_FORCE_SYSTEM
    "Force system packages for all CPM dependencies (NOT RECOMMENDED)" OFF)

cmake_minimum_required(VERSION 3.22)
include(CPM)

# TODO(crueter): Better solution for separate cpmfiles e.g. per-directory
set(CPMUTIL_JSON_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cpmfile.json" CACHE STRING "Location of cpmfile.json")

if (EXISTS ${CPMUTIL_JSON_FILE})
    file(READ ${CPMUTIL_JSON_FILE} CPMFILE_CONTENT)
else()
    message(WARNING "[CPMUtil] cpmfile ${CPMUTIL_JSON_FILE} does not exist, AddJsonPackage will be a no-op")
endif()

# utility
function(cpm_utils_message level name message)
    message(${level} "[CPMUtil] ${name}: ${message}")
endfunction()

# utility
function(array_to_list array length out)
    math(EXPR range "${length} - 1")

    foreach(IDX RANGE ${range})
        string(JSON _element GET "${array}" "${IDX}")

        list(APPEND NEW_LIST ${_element})
    endforeach()

    set("${out}" "${NEW_LIST}" PARENT_SCOPE)
endfunction()

# utility
function(get_json_element object out member default)
    string(JSON out_type ERROR_VARIABLE err TYPE "${object}" ${member})

    if (err)
        set("${out}" "${default}" PARENT_SCOPE)
        return()
    endif()

    string(JSON outvar GET "${object}" ${member})

    if (out_type STREQUAL "ARRAY")
        string(JSON _len LENGTH "${object}" ${member})
        # array_to_list("${outvar}" ${_len} outvar)
        set("${out}_LENGTH" "${_len}" PARENT_SCOPE)
    endif()

    set("${out}" "${outvar}" PARENT_SCOPE)
endfunction()

# Kinda cancerous but whatever
function(AddJsonPackage)
    set(oneValueArgs
        NAME

        # these are overrides that can be generated at runtime, so can be defined separately from the json
        DOWNLOAD_ONLY
        SYSTEM_PACKAGE
        BUNDLED_PACKAGE
    )

    set(multiValueArgs OPTIONS)

    cmake_parse_arguments(JSON "" "${oneValueArgs}" "${multiValueArgs}"
                          "${ARGN}")

    list(LENGTH ARGN argnLength)
    # single name argument
    if(argnLength EQUAL 1)
        set(JSON_NAME "${ARGV0}")
    endif()

    if (NOT DEFINED CPMFILE_CONTENT)
        cpm_utils_message(WARNING ${name} "No cpmfile, AddJsonPackage is a no-op")
        return()
    endif()

    if (NOT DEFINED JSON_NAME)
        cpm_utils_message(FATAL_ERROR "json package" "No name specified")
    endif()

    string(JSON object ERROR_VARIABLE err GET "${CPMFILE_CONTENT}" "${JSON_NAME}")

    if (err)
        cpm_utils_message(FATAL_ERROR ${JSON_NAME} "Not found in cpmfile")
    endif()

    get_json_element("${object}" package package ${JSON_NAME})
    get_json_element("${object}" repo repo "")
    get_json_element("${object}" ci ci OFF)
    get_json_element("${object}" version version "")

    if (ci)
        get_json_element("${object}" name name "${JSON_NAME}")
        get_json_element("${object}" extension extension "tar.zst")
        get_json_element("${object}" min_version min_version "")
        get_json_element("${object}" cmake_filename cmake_filename "")
        get_json_element("${object}" raw_disabled disabled_platforms "")

        if (raw_disabled)
            array_to_list("${raw_disabled}" ${raw_disabled_LENGTH} disabled_platforms)
        else()
            set(disabled_platforms "")
        endif()

        AddCIPackage(
            VERSION ${version}
            NAME ${name}
            REPO ${repo}
            PACKAGE ${package}
            EXTENSION ${extension}
            MIN_VERSION ${min_version}
            DISABLED_PLATFORMS ${disabled_platforms}
            CMAKE_FILENAME ${cmake_filename}
        )
        return()
    endif()

    get_json_element("${object}" hash hash "")
    get_json_element("${object}" sha sha "")
    get_json_element("${object}" url url "")
    get_json_element("${object}" key key "")
    get_json_element("${object}" tag tag "")
    get_json_element("${object}" artifact artifact "")
    get_json_element("${object}" git_version git_version "")
    get_json_element("${object}" source_subdir source_subdir "")
    get_json_element("${object}" bundled bundled "unset")
    get_json_element("${object}" find_args find_args "")
    get_json_element("${object}" raw_patches patches "")

    # format patchdir
    if (raw_patches)
        math(EXPR range "${raw_patches_LENGTH} - 1")

        foreach(IDX RANGE ${range})
            string(JSON _patch GET "${raw_patches}" "${IDX}")

            set(full_patch "${CMAKE_SOURCE_DIR}/.patch/${JSON_NAME}/${_patch}")
            if (NOT EXISTS ${full_patch})
                cpm_utils_message(FATAL_ERROR ${JSON_NAME} "specifies patch ${full_patch} which does not exist")
            endif()

            list(APPEND patches "${full_patch}")
        endforeach()
    endif()
    # end format patchdir

    # options
    get_json_element("${object}" raw_options options "")

    if (raw_options)
        array_to_list("${raw_options}" ${raw_options_LENGTH} options)
    endif()

    set(options ${options} ${JSON_OPTIONS})

    # end options

    # system/bundled
    if (bundled STREQUAL "unset" AND DEFINED JSON_BUNDLED_PACKAGE)
        set(bundled ${JSON_BUNDLED_PACKAGE})
    else()
        set(bundled ON)
    endif()

    AddPackage(
        NAME "${package}"
        VERSION "${version}"
        URL "${url}"
        HASH "${hash}"
        SHA "${sha}"
        REPO "${repo}"
        KEY "${key}"
        PATCHES "${patches}"
        OPTIONS "${options}"
        FIND_PACKAGE_ARGUMENTS "${find_args}"
        BUNDLED_PACKAGE "${bundled}"
        SOURCE_SUBDIR "${source_subdir}"

        GIT_VERSION ${git_version}
        ARTIFACT ${artifact}
        TAG ${tag}
    )

    # pass stuff to parent scope
    set(${package}_ADDED "${${package}_ADDED}"
        PARENT_SCOPE)
    set(${package}_SOURCE_DIR "${${package}_SOURCE_DIR}"
        PARENT_SCOPE)
    set(${package}_BINARY_DIR "${${package}_BINARY_DIR}"
        PARENT_SCOPE)

endfunction()

function(AddPackage)
    cpm_set_policies()

    # TODO(crueter): docs, git clone

    #[[
        URL configurations, descending order of precedence:
        - URL [+ GIT_URL] -> bare URL fetch
        - REPO + TAG + ARTIFACT -> github release artifact
        - REPO + TAG -> github release archive
        - REPO + SHA -> github commit archive
        - REPO + BRANCH -> github branch

        Hash configurations, descending order of precedence:
        - HASH -> bare sha512sum
        - HASH_SUFFIX -> hash grabbed from the URL + this suffix
        - HASH_URL -> hash grabbed from a URL
          * technically this is unsafe since a hacker can attack that url

        NOTE: hash algo defaults to sha512
    #]]
    set(oneValueArgs
        NAME
        VERSION
        GIT_VERSION

        REPO
        TAG
        ARTIFACT
        SHA
        BRANCH

        HASH
        HASH_SUFFIX
        HASH_URL
        HASH_ALGO

        URL
        GIT_URL

        KEY
        BUNDLED_PACKAGE
    )

    set(multiValueArgs OPTIONS PATCHES)

    cmake_parse_arguments(PKG_ARGS "" "${oneValueArgs}" "${multiValueArgs}"
                          "${ARGN}")

    if (NOT DEFINED PKG_ARGS_NAME)
        cpm_utils_message(FATAL_ERROR "package" "No package name defined")
    endif()

    option(${PKG_ARGS_NAME}_FORCE_SYSTEM "Force the system package for ${PKG_ARGS_NAME}")
    option(${PKG_ARGS_NAME}_FORCE_BUNDLED "Force the bundled package for ${PKG_ARGS_NAME}")

    if (DEFINED PKG_ARGS_URL)
        set(pkg_url ${PKG_ARGS_URL})

        if (DEFINED PKG_ARGS_REPO)
            set(pkg_git_url https://github.com/${PKG_ARGS_REPO})
        else()
            if (DEFINED PKG_ARGS_GIT_URL)
                set(pkg_git_url ${PKG_ARGS_GIT_URL})
            else()
                set(pkg_git_url ${pkg_url})
            endif()
        endif()
    elseif (DEFINED PKG_ARGS_REPO)
        set(pkg_git_url https://github.com/${PKG_ARGS_REPO})

        if (DEFINED PKG_ARGS_TAG)
            set(pkg_key ${PKG_ARGS_TAG})

            if(DEFINED PKG_ARGS_ARTIFACT)
                set(pkg_url
                    ${pkg_git_url}/releases/download/${PKG_ARGS_TAG}/${PKG_ARGS_ARTIFACT})
            else()
                set(pkg_url
                    ${pkg_git_url}/archive/refs/tags/${PKG_ARGS_TAG}.tar.gz)
            endif()
        elseif (DEFINED PKG_ARGS_SHA)
            set(pkg_url "${pkg_git_url}/archive/${PKG_ARGS_SHA}.zip")
        else()
            if (DEFINED PKG_ARGS_BRANCH)
                set(PKG_BRANCH ${PKG_ARGS_BRANCH})
            else()
                cpm_utils_message(WARNING ${PKG_ARGS_NAME}
                            "REPO defined but no TAG, SHA, BRANCH, or URL specified, defaulting to master")
                set(PKG_BRANCH master)
            endif()

            set(pkg_url ${pkg_git_url}/archive/refs/heads/${PKG_BRANCH}.zip)
        endif()
    else()
        cpm_utils_message(FATAL_ERROR ${PKG_ARGS_NAME} "No URL or repository defined")
    endif()

    cpm_utils_message(STATUS ${PKG_ARGS_NAME} "Download URL is ${pkg_url}")

    if (DEFINED PKG_ARGS_GIT_VERSION)
        set(git_version ${PKG_ARGS_GIT_VERSION})
    elseif(DEFINED PKG_ARGS_VERSION)
        set(git_version ${PKG_ARGS_VERSION})
    endif()

    if (NOT DEFINED PKG_ARGS_KEY)
        if (DEFINED PKG_ARGS_SHA)
            string(SUBSTRING ${PKG_ARGS_SHA} 0 4 pkg_key)
            cpm_utils_message(DEBUG ${PKG_ARGS_NAME}
                        "No custom key defined, using ${pkg_key} from sha")
        elseif (DEFINED git_version)
            set(pkg_key ${git_version})
            cpm_utils_message(DEBUG ${PKG_ARGS_NAME}
                        "No custom key defined, using ${pkg_key}")
        elseif (DEFINED PKG_ARGS_TAG)
            set(pkg_key ${PKG_ARGS_TAG})
            cpm_utils_message(DEBUG ${PKG_ARGS_NAME}
                        "No custom key defined, using ${pkg_key}")
        else()
            cpm_utils_message(WARNING ${PKG_ARGS_NAME}
                        "Could not determine cache key, using CPM defaults")
        endif()
    else()
        set(pkg_key ${PKG_ARGS_KEY})
    endif()

    if (DEFINED PKG_ARGS_HASH_ALGO)
        set(hash_algo ${PKG_ARGS_HASH_ALGO})
    else()
        set(hash_algo SHA512)
    endif()

    if (DEFINED PKG_ARGS_HASH)
        set(pkg_hash "${hash_algo}=${PKG_ARGS_HASH}")
    elseif (DEFINED PKG_ARGS_HASH_SUFFIX)
        # funny sanity check
        string(TOLOWER ${hash_algo} hash_algo_lower)
        string(TOLOWER ${PKG_ARGS_HASH_SUFFIX} suffix_lower)
        if (NOT ${suffix_lower} MATCHES ${hash_algo_lower})
            cpm_utils_message(WARNING
                        "Hash algorithm and hash suffix do not match, errors may occur")
        endif()

        set(hash_url ${pkg_url}.${PKG_ARGS_HASH_SUFFIX})
    elseif (DEFINED PKG_ARGS_HASH_URL)
        set(hash_url ${PKG_ARGS_HASH_URL})
    else()
        cpm_utils_message(WARNING ${PKG_ARGS_NAME}
                    "No hash or hash URL found")
    endif()

    if (DEFINED hash_url)
        set(outfile ${CMAKE_CURRENT_BINARY_DIR}/${PKG_ARGS_NAME}.hash)

        # TODO(crueter): This is kind of a bad solution
        # because "technically" the hash is invalidated each week
        # but it works for now kjsdnfkjdnfjksdn
        string(TOLOWER ${PKG_ARGS_NAME} lowername)
        if (NOT EXISTS ${outfile} AND NOT EXISTS ${CPM_SOURCE_CACHE}/${lowername}/${pkg_key})
            file(DOWNLOAD ${hash_url} ${outfile})
        endif()

        if (EXISTS ${outfile})
            file(READ ${outfile} pkg_hash_tmp)
        endif()

        if (DEFINED ${pkg_hash_tmp})
            set(pkg_hash "${hash_algo}=${pkg_hash_tmp}")
        endif()
    endif()

    macro(set_precedence local force)
        set(CPM_USE_LOCAL_PACKAGES ${local})
        set(CPM_LOCAL_PACKAGES_ONLY ${force})
    endmacro()

    #[[
        Precedence:
        - package_FORCE_SYSTEM
        - package_FORCE_BUNDLED
        - CPMUTIL_FORCE_SYSTEM
        - CPMUTIL_FORCE_BUNDLED
        - BUNDLED_PACKAGE
        - default to allow local
    ]]#
    if (${PKG_ARGS_NAME}_FORCE_SYSTEM)
        set_precedence(ON ON)
    elseif (${PKG_ARGS_NAME}_FORCE_BUNDLED)
        set_precedence(OFF OFF)
    elseif (CPMUTIL_FORCE_SYSTEM)
        set_precedence(ON ON)
    elseif(NOT CPMUTIL_FORCE_BUNDLED)
        set_precedence(OFF OFF)
    elseif (DEFINED PKG_ARGS_BUNDLED_PACKAGE)
        if (PKG_ARGS_BUNDLED_PACKAGE)
            set(local OFF)
        else()
            set(local ON)
        endif()

        set_precedence(${local} OFF)
    else()
        set_precedence(ON OFF)
    endif()

    CPMAddPackage(
        NAME ${PKG_ARGS_NAME}
        VERSION ${PKG_ARGS_VERSION}
        URL ${pkg_url}
        URL_HASH ${pkg_hash}
        CUSTOM_CACHE_KEY ${pkg_key}
        DOWNLOAD_ONLY ${PKG_ARGS_DOWNLOAD_ONLY}
        FIND_PACKAGE_ARGUMENTS ${PKG_ARGS_FIND_PACKAGE_ARGUMENTS}

        OPTIONS ${PKG_ARGS_OPTIONS}
        PATCHES ${PKG_ARGS_PATCHES}
        EXCLUDE_FROM_ALL ON

        ${PKG_ARGS_UNPARSED_ARGUMENTS}
    )

    set_property(GLOBAL APPEND PROPERTY CPM_PACKAGE_NAMES ${PKG_ARGS_NAME})
    set_property(GLOBAL APPEND PROPERTY CPM_PACKAGE_URLS ${pkg_git_url})

    if (${PKG_ARGS_NAME}_ADDED)
        if (DEFINED PKG_ARGS_SHA)
            set_property(GLOBAL APPEND PROPERTY CPM_PACKAGE_SHAS
                         ${PKG_ARGS_SHA})
        elseif(DEFINED git_version)
            set_property(GLOBAL APPEND PROPERTY CPM_PACKAGE_SHAS
                         ${git_version})
        elseif (DEFINED PKG_ARGS_TAG)
            set_property(GLOBAL APPEND PROPERTY CPM_PACKAGE_SHAS
                         ${PKG_ARGS_TAG})
        else()
            cpm_utils_message(WARNING ${PKG_ARGS_NAME}
                        "Package has no specified sha, tag, or version")
            set_property(GLOBAL APPEND PROPERTY CPM_PACKAGE_SHAS "unknown")
        endif()
    else()
        if (DEFINED CPM_PACKAGE_${PKG_ARGS_NAME}_VERSION AND NOT
            "${CPM_PACKAGE_${PKG_ARGS_NAME}_VERSION}" STREQUAL "")
            set_property(GLOBAL APPEND PROPERTY CPM_PACKAGE_SHAS
                         "${CPM_PACKAGE_${PKG_ARGS_NAME}_VERSION} (system)")
        else()
            set_property(GLOBAL APPEND PROPERTY CPM_PACKAGE_SHAS
                         "unknown (system)")
        endif()
    endif()

    # pass stuff to parent scope
    set(${PKG_ARGS_NAME}_ADDED "${${PKG_ARGS_NAME}_ADDED}"
        PARENT_SCOPE)
    set(${PKG_ARGS_NAME}_SOURCE_DIR "${${PKG_ARGS_NAME}_SOURCE_DIR}"
        PARENT_SCOPE)
    set(${PKG_ARGS_NAME}_BINARY_DIR "${${PKG_ARGS_NAME}_BINARY_DIR}"
        PARENT_SCOPE)

endfunction()

function(add_ci_package key)
    set(ARTIFACT ${ARTIFACT_NAME}-${key}-${ARTIFACT_VERSION}.${ARTIFACT_EXT})

    AddPackage(
        NAME ${ARTIFACT_PACKAGE}
        REPO ${ARTIFACT_REPO}
        TAG v${ARTIFACT_VERSION}
        VERSION ${ARTIFACT_VERSION}
        ARTIFACT ${ARTIFACT}

        KEY ${key}
        HASH_SUFFIX sha512sum
        BUNDLED_PACKAGE ON
    )

    set(ARTIFACT_DIR ${${ARTIFACT_PACKAGE}_SOURCE_DIR} PARENT_SCOPE)
endfunction()

# name is the artifact name, package is for find_package override
function(AddCIPackage)
    set(oneValueArgs
        VERSION
        NAME
        REPO
        PACKAGE
        EXTENSION
        MIN_VERSION
        DISABLED_PLATFORMS
        CMAKE_FILENAME
    )

    cmake_parse_arguments(PKG_ARGS "" "${oneValueArgs}" "" ${ARGN})

    if(NOT DEFINED PKG_ARGS_VERSION)
        message(FATAL_ERROR "[CPMUtil] VERSION is required")
    endif()
    if(NOT DEFINED PKG_ARGS_NAME)
        message(FATAL_ERROR "[CPMUtil] NAME is required")
    endif()
    if(NOT DEFINED PKG_ARGS_REPO)
        message(FATAL_ERROR "[CPMUtil] REPO is required")
    endif()
    if(NOT DEFINED PKG_ARGS_PACKAGE)
        message(FATAL_ERROR "[CPMUtil] PACKAGE is required")
    endif()

    if (NOT DEFINED PKG_ARGS_CMAKE_FILENAME)
        set(ARTIFACT_CMAKE ${PKG_ARGS_NAME})
    else()
        set(ARTIFACT_CMAKE ${PKG_ARGS_CMAKE_FILENAME})
    endif()

    if(NOT DEFINED PKG_ARGS_EXTENSION)
        set(ARTIFACT_EXT "tar.zst")
    else()
        set(ARTIFACT_EXT ${PKG_ARGS_EXTENSION})
    endif()

    if (DEFINED PKG_ARGS_MIN_VERSION)
        set(ARTIFACT_MIN_VERSION ${PKG_ARGS_MIN_VERSION})
    endif()

    if (DEFINED PKG_ARGS_DISABLED_PLATFORMS)
        set(DISABLED_PLATFORMS ${PKG_ARGS_DISABLED_PLATFORMS})
    endif()

    # this is mildly annoying
    set(ARTIFACT_VERSION ${PKG_ARGS_VERSION})
    set(ARTIFACT_NAME ${PKG_ARGS_NAME})
    set(ARTIFACT_REPO ${PKG_ARGS_REPO})
    set(ARTIFACT_PACKAGE ${PKG_ARGS_PACKAGE})

    if ((MSVC AND ARCHITECTURE_x86_64) AND NOT "windows-amd64" IN_LIST DISABLED_PLATFORMS)
        add_ci_package(windows-amd64)
    endif()

    if ((MSVC AND ARCHITECTURE_arm64) AND NOT "windows-arm64" IN_LIST DISABLED_PLATFORMS)
        add_ci_package(windows-arm64)
    endif()

    if (ANDROID AND NOT "android" IN_LIST DISABLED_PLATFORMS)
        add_ci_package(android)
    endif()

    if(PLATFORM_SUN AND NOT "solaris" IN_LIST DISABLED_PLATFORMS)
        add_ci_package(solaris)
    endif()

    if(PLATFORM_FREEBSD AND NOT "freebsd" IN_LIST DISABLED_PLATFORMS)
        add_ci_package(freebsd)
    endif()

    if((PLATFORM_LINUX AND ARCHITECTURE_x86_64) AND NOT "linux" IN_LIST DISABLED_PLATFORMS)
        add_ci_package(linux)
    endif()

    if((PLATFORM_LINUX AND ARCHITECTURE_arm64) AND NOT "linux-aarch64" IN_LIST DISABLED_PLATFORMS)
        add_ci_package(linux-aarch64)
    endif()

    if (DEFINED ARTIFACT_DIR)
        include(${ARTIFACT_DIR}/${ARTIFACT_CMAKE}.cmake)

        set_property(GLOBAL APPEND PROPERTY CPM_PACKAGE_NAMES ${ARTIFACT_NAME})
        set_property(GLOBAL APPEND PROPERTY CPM_PACKAGE_URLS "https://github.com/${ARTIFACT_REPO}") # TODO(crueter) other hosts?
        set_property(GLOBAL APPEND PROPERTY CPM_PACKAGE_SHAS ${ARTIFACT_VERSION})

        set(${ARTIFACT_PACKAGE}_ADDED TRUE PARENT_SCOPE)
    else()
        find_package(${ARTIFACT_PACKAGE} ${ARTIFACT_MIN_VERSION} REQUIRED)
    endif()
endfunction()
