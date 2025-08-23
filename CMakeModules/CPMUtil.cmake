# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# Created-By: crueter
# Docs will come at a later date, mostly this is to just reduce boilerplate
# and some cmake magic to allow for runtime viewing of dependency versions

include(CMakeDependentOption)
if (MSVC OR ANDROID)
    set(SYSTEM_DEFAULT OFF)
else()
    set(SYSTEM_DEFAULT ON)
endif()

CMAKE_DEPENDENT_OPTION(CPMUTIL_DEFAULT_SYSTEM
                       "Allow usage of system packages for CPM dependencies" ${SYSTEM_DEFAULT}
                       "NOT ANDROID" OFF)

cmake_minimum_required(VERSION 3.22)
include(CPM)

function(cpm_utils_message level name message)
    message(${level} "[CPMUtil] ${name}: ${message}")
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
        DOWNLOAD_ONLY
        FIND_PACKAGE_ARGUMENTS
        SYSTEM_PACKAGE
        BUNDLED_PACKAGE
    )

    set(multiValueArgs OPTIONS PATCHES)

    cmake_parse_arguments(PKG_ARGS "" "${oneValueArgs}" "${multiValueArgs}"
                          "${ARGN}")

    if (NOT DEFINED PKG_ARGS_NAME)
        cpm_utils_message(FATAL_ERROR "package" "No package name defined")
    endif()

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
        set(git_version ${PKG_ARGS_VERSION})
    elseif(DEFINED PKG_ARGS_VERSION)
        set(git_version ${PKG_ARGS_GIT_VERSION})
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

        file(DOWNLOAD ${hash_url} ${outfile})
        file(READ ${outfile} pkg_hash_tmp)
        file(REMOVE ${outfile})

        set(pkg_hash "${hash_algo}=${pkg_hash_tmp}")
    endif()

    if (NOT CPMUTIL_DEFAULT_SYSTEM)
        set(CPM_USE_LOCAL_PACKAGES OFF)
    elseif (DEFINED PKG_ARGS_SYSTEM_PACKAGE)
        set(CPM_USE_LOCAL_PACKAGES ${PKG_ARGS_SYSTEM_PACKAGE})
    elseif (DEFINED PKG_ARGS_BUNDLED_PACKAGE)
        if (PKG_ARGS_BUNDLED_PACKAGE)
            set(CPM_USE_LOCAL_PACKAGES OFF)
        else()
            set(CPM_USE_LOCAL_PACKAGES ON)
        endif()
    else()
        set(CPM_USE_LOCAL_PACKAGES ON)
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
        NAME ${ARTIFACT_PACKAGE}-${key}
        REPO ${ARTIFACT_REPO}
        TAG v${ARTIFACT_VERSION}
        VERSION ${ARTIFACT_VERSION}
        ARTIFACT ${ARTIFACT}

        KEY ${key}
        HASH_SUFFIX sha512sum
        BUNDLED_PACKAGE ON
        DOWNLOAD_ONLY ON
    )

    if (NOT ARTIFACT_FORCE_DOWNLOAD OR ARTIFACT_OVERRIDE)
        set(ARTIFACT_DIR ${${ARTIFACT_PACKAGE}-${key}_SOURCE_DIR} PARENT_SCOPE)
    endif()
endfunction()

# TODO(crueter): doc
function(AddCIPackage)
    set(oneValueArgs
        VERSION
        NAME
        REPO
        PACKAGE
        EXTENSION
        FORCE_DOWNLOAD
        MIN_VERSION
        DISABLED_PLATFORMS
        CMAKE_FILENAME
        TARGET
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

    if(NOT DEFINED PKG_ARGS_FORCE_DOWNLOAD)
        set(ARTIFACT_FORCE_DOWNLOAD OFF)
    else()
        set(ARTIFACT_FORCE_DOWNLOAD ${PKG_ARGS_FORCE_DOWNLOAD})
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

    if ((MSVC AND ARCHITECTURE_x86_64) OR ARTIFACT_FORCE_DOWNLOAD AND NOT "windows-amd64" IN_LIST DISABLED_PLATFORMS)
        # kinda hacky
        if(MSVC AND ARCHITECTURE_x86_64)
            set(ARTIFACT_OVERRIDE ON)
        endif()

        add_ci_package(windows-amd64)
        set(ARTIFACT_OVERRIDE OFF)
    endif()

    if ((MSVC AND ARCHITECTURE_arm64) OR ARTIFACT_FORCE_DOWNLOAD AND NOT "windows-arm64" IN_LIST DISABLED_PLATFORMS)
        if(MSVC AND ARCHITECTURE_arm64)
            set(ARTIFACT_OVERRIDE ON)
        endif()

        add_ci_package(windows-arm64)
        set(ARTIFACT_OVERRIDE OFF)
    endif()

    if (ANDROID OR ARTIFACT_FORCE_DOWNLOAD AND NOT "android" IN_LIST DISABLED_PLATFORMS)
        if(ANDROID)
            set(ARTIFACT_OVERRIDE ON)
        endif()

        add_ci_package(android)
        set(ARTIFACT_OVERRIDE OFF)
    endif()

    if(PLATFORM_SUN OR ARTIFACT_FORCE_DOWNLOAD AND NOT "solaris" IN_LIST DISABLED_PLATFORMS)
        if(PLATFORM_SUN)
            set(ARTIFACT_OVERRIDE ON)
        endif()

        add_ci_package(solaris)
        set(ARTIFACT_OVERRIDE OFF)
    endif()

    if(PLATFORM_FREEBSD OR ARTIFACT_FORCE_DOWNLOAD AND NOT "freebsd" IN_LIST DISABLED_PLATFORMS)
        if(PLATFORM_FREEBSD)
            set(ARTIFACT_OVERRIDE ON)
        endif()

        add_ci_package(freebsd)
        set(ARTIFACT_OVERRIDE OFF)
    endif()

    if((PLATFORM_LINUX AND ARCHITECTURE_x86_64) OR ARTIFACT_FORCE_DOWNLOAD AND NOT "linux" IN_LIST DISABLED_PLATFORMS)
        if(PLATFORM_LINUX AND ARCHITECTURE_x86_64)
            set(ARTIFACT_OVERRIDE ON)
        endif()

        add_ci_package(linux)
        set(ARTIFACT_OVERRIDE OFF)
    endif()

    if((PLATFORM_LINUX AND ARCHITECTURE_arm64) OR ARTIFACT_FORCE_DOWNLOAD AND NOT "linux-aarch64" IN_LIST DISABLED_PLATFORMS)
        if(PLATFORM_LINUX AND ARCHITECTURE_arm64)
            set(ARTIFACT_OVERRIDE ON)
        endif()

        add_ci_package(linux-aarch64)
        set(ARTIFACT_OVERRIDE OFF)
    endif()

    if (DEFINED ARTIFACT_DIR)
        if (NOT DEFINED PKG_ARGS_TARGET OR NOT TARGET "${PKG_ARGS_TARGET}")
            include(${ARTIFACT_DIR}/${ARTIFACT_CMAKE}.cmake)

            # Overrides find package
            CPMAddPackage(
                NAME ${ARTIFACT_PACKAGE}
                SOURCE_DIR ${ARTIFACT_DIR}
            )

            set_property(GLOBAL APPEND PROPERTY CPM_PACKAGE_NAMES ${ARTIFACT_NAME})
            set_property(GLOBAL APPEND PROPERTY CPM_PACKAGE_URLS "https://github.com/${ARTIFACT_REPO}") # TODO(crueter) other hosts?
            set_property(GLOBAL APPEND PROPERTY CPM_PACKAGE_SHAS ${ARTIFACT_VERSION})

            set(${ARTIFACT_PACKAGE}_ADDED TRUE PARENT_SCOPE)
        endif()
    else()
        find_package(${ARTIFACT_PACKAGE} ${ARTIFACT_MIN_VERSION} REQUIRED)
    endif()
endfunction()
