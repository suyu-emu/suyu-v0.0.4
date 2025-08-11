# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# Created-By: crueter
# Docs will come at a later date, mostly this is to just reduce boilerplate
# and some cmake magic to allow for runtime viewing of dependency versions

cmake_minimum_required(VERSION 3.22)
include(CPM)

function(AddPackage)
    cpm_set_policies()

    set(oneValueArgs
        NAME
        VERSION
        REPO
        SHA
        HASH
        KEY
        URL # Only for custom non-GitHub urls
        DOWNLOAD_ONLY
        FIND_PACKAGE_ARGUMENTS
        SYSTEM_PACKAGE
        BUNDLED_PACKAGE
    )

    set(multiValueArgs OPTIONS PATCHES)

    cmake_parse_arguments(PKG_ARGS "" "${oneValueArgs}" "${multiValueArgs}" "${ARGN}")

    if (NOT DEFINED PKG_ARGS_NAME)
        message(FATAL_ERROR "CPMUtil: No package name defined")
    endif()

    if (NOT DEFINED PKG_ARGS_URL)
        if (DEFINED PKG_ARGS_REPO AND DEFINED PKG_ARGS_SHA)
            set(PKG_GIT_URL https://github.com/${PKG_ARGS_REPO})
            set(PKG_URL "${PKG_GIT_URL}/archive/${PKG_ARGS_SHA}.zip")
        else()
            message(FATAL_ERROR "CPMUtil: No custom URL and no repository + sha defined")
        endif()
    else()
        set(PKG_URL ${PKG_ARGS_URL})
        set(PKG_GIT_URL ${PKG_URL})
    endif()

    message(STATUS "CPMUtil: Downloading package ${PKG_ARGS_NAME} from ${PKG_URL}")

    if (NOT DEFINED PKG_ARGS_KEY)
        if (DEFINED PKG_ARGS_SHA)
            string(SUBSTRING ${PKG_ARGS_SHA} 0 4 PKG_KEY)
            message(STATUS "CPMUtil: No custom key defined, using ${PKG_KEY} from sha")
        else()
            message(FATAL_ERROR "CPMUtil: No custom key and no commit sha defined")
        endif()
    else()
        set(PKG_KEY ${PKG_ARGS_KEY})
    endif()

    if (DEFINED PKG_ARGS_HASH)
        set(PKG_HASH "SHA512=${PKG_ARGS_HASH}")
    endif()

    # Default behavior is bundled
    if (DEFINED PKG_ARGS_SYSTEM_PACKAGE)
        set(CPM_USE_LOCAL_PACKAGES ${PKG_ARGS_SYSTEM_PACKAGE})
    elseif (DEFINED PKG_ARGS_BUNDLED_PACKAGE)
        if (PKG_ARGS_BUNDLED_PACKAGE)
            set(CPM_USE_LOCAL_PACKAGES OFF)
        else()
            set(CPM_USE_LOCAL_PACKAGES ON)
        endif()
    else()
        set(CPM_USE_LOCAL_PACKAGES OFF)
    endif()

    CPMAddPackage(
        NAME ${PKG_ARGS_NAME}
        VERSION ${PKG_ARGS_VERSION}
        URL ${PKG_URL}
        URL_HASH ${PKG_HASH}
        CUSTOM_CACHE_KEY ${PKG_KEY}
        DOWNLOAD_ONLY ${PKG_ARGS_DOWNLOAD_ONLY}
        FIND_PACKAGE_ARGUMENTS ${PKG_ARGS_FIND_PACKAGE_ARGUMENTS}

        OPTIONS ${PKG_ARGS_OPTIONS}
        PATCHES ${PKG_ARGS_PATCHES}

        ${PKG_ARGS_UNPARSED_ARGUMENTS}
    )

    set_property(GLOBAL APPEND PROPERTY CPM_PACKAGE_NAMES ${PKG_ARGS_NAME})
    set_property(GLOBAL APPEND PROPERTY CPM_PACKAGE_URLS ${PKG_GIT_URL})

    if (${PKG_ARGS_NAME}_ADDED)
        if (DEFINED PKG_ARGS_SHA)
            set_property(GLOBAL APPEND PROPERTY CPM_PACKAGE_SHAS ${PKG_ARGS_SHA})
        elseif(DEFINED PKG_ARGS_VERSION)
            set_property(GLOBAL APPEND PROPERTY CPM_PACKAGE_SHAS ${PKG_ARGS_VERSION})
        else()
            message(WARNING "CPMUtil: Package ${PKG_ARGS_NAME} has no specified sha or version")
            set_property(GLOBAL APPEND PROPERTY CPM_PACKAGE_SHAS "unknown")
        endif()
    else()
        if (DEFINED CPM_PACKAGE_${PKG_ARGS_NAME}_VERSION)
            set_property(GLOBAL APPEND PROPERTY CPM_PACKAGE_SHAS "${CPM_PACKAGE_${PKG_ARGS_NAME}_VERSION} (system)")
        else()
            set_property(GLOBAL APPEND PROPERTY CPM_PACKAGE_SHAS "unknown (system)")
        endif()
    endif()

    # pass stuff to parent scope
    set(${PKG_ARGS_NAME}_ADDED "${${PKG_ARGS_NAME}_ADDED}" PARENT_SCOPE)
    set(${PKG_ARGS_NAME}_SOURCE_DIR "${${PKG_ARGS_NAME}_SOURCE_DIR}" PARENT_SCOPE)
    set(${PKG_ARGS_NAME}_BINARY_DIR "${${PKG_ARGS_NAME}_BINARY_DIR}" PARENT_SCOPE)
endfunction()
