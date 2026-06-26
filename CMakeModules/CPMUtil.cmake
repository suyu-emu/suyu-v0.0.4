# SPDX-FileCopyrightText: Copyright 2026 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

cmake_minimum_required(VERSION 3.31)

if(MSVC OR ANDROID OR IOS)
    set(BUNDLED_DEFAULT ON)
else()
    set(BUNDLED_DEFAULT OFF)
endif()

set(CPM_SOURCE_CACHE "${PROJECT_SOURCE_DIR}/.cache/cpm" CACHE STRING "" FORCE)

option(CPMUTIL_FORCE_BUNDLED
    "Force bundled packages for all CPM depdendencies" ${BUNDLED_DEFAULT})

option(CPMUTIL_FORCE_SYSTEM
    "Force system packages for all CPM dependencies" OFF)

set(CPMUTIL_PATCH_DIR "${PROJECT_SOURCE_DIR}/.patch" CACHE STRING
    "Directory containing patches for packages")

include(CPM)

# Rudimentary target architecture detection
if (NOT DEFINED ARCHITECTURE)
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
set(CPMUTIL_JSON_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cpmfile.json")

if(EXISTS ${CPMUTIL_JSON_FILE})
    file(READ ${CPMUTIL_JSON_FILE} CPMFILE_CONTENT)
    if (NOT TARGET cpmfiles)
        add_custom_target(cpmfiles)
    endif()

    target_sources(cpmfiles PRIVATE ${CPMUTIL_JSON_FILE})
    set_property(DIRECTORY APPEND PROPERTY
        CMAKE_CONFIGURE_DEPENDS
        "${CPMUTIL_JSON_FILE}")
else()
    message(DEBUG "[CPMUtil] cpmfile ${CPMUTIL_JSON_FILE}"
        "does not exist, AddJsonPackage will be a no-op")
endif()

# Utility stuff
function(cpm_utils_message level name message)
    message(${level} "[CPMUtil] ${name}: ${message}")
endfunction()

# propagate a variable to parent scope
macro(Propagate var)
    set(${var} ${${var}} PARENT_SCOPE)
endmacro()

function(array_to_list array length out)
    math(EXPR range "${length} - 1")

    foreach(IDX RANGE ${range})
        string(JSON _element GET "${array}" "${IDX}")

        list(APPEND NEW_LIST ${_element})
    endforeach()

    set("${out}" "${NEW_LIST}" PARENT_SCOPE)
endfunction()

function(get_json_element object out member default)
    string(JSON out_type ERROR_VARIABLE err TYPE "${object}" ${member})

    if(err)
        set("${out}" "${default}" PARENT_SCOPE)
        return()
    endif()

    string(JSON outvar GET "${object}" ${member})

    if(out_type STREQUAL "ARRAY")
        string(JSON _len LENGTH "${object}" ${member})
        set("${out}_LENGTH" "${_len}" PARENT_SCOPE)
    endif()

    set("${out}" "${outvar}" PARENT_SCOPE)
endfunction()

# Determine whether or not a package has a viable system candidate.
function(SystemPackageViable JSON_NAME)
    string(JSON object GET "${CPMFILE_CONTENT}" "${JSON_NAME}")

    parse_object(${object})

    string(REPLACE " " ";" find_args "${find_args}")
    if (${package}_FORCE_BUNDLED)
        set(${package}_FOUND OFF)
    else()
        find_package(${package} ${version} ${find_args} QUIET NO_POLICY_SCOPE)
    endif()

    set(${pkg}_VIABLE ${${package}_FOUND} PARENT_SCOPE)
    set(${pkg}_PACKAGE ${package} PARENT_SCOPE)
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

# json util
macro(parse_object object)
    get_json_element("${object}" package package ${JSON_NAME})
    get_json_element("${object}" repo repo "")
    get_json_element("${object}" ci ci OFF)
    get_json_element("${object}" version version "")
    get_json_element("${object}" min_version min_version "")
    get_json_element("${object}" git_host git_host "github.com")

    if(ci)
        get_json_element("${object}" name name "${JSON_NAME}")
        get_json_element("${object}" extension extension "tar.zst")
        get_json_element("${object}" raw_disabled disabled_platforms "")

        if(raw_disabled)
            array_to_list("${raw_disabled}"
                ${raw_disabled_LENGTH} disabled_platforms)
        else()
            set(disabled_platforms "")
        endif()
    else()
        get_json_element("${object}" hash hash "")
        get_json_element("${object}" sha sha "")
        get_json_element("${object}" url url "")
        get_json_element("${object}" tag tag "")
        get_json_element("${object}" artifact artifact "")
        get_json_element("${object}" source_subdir source_subdir "")
        get_json_element("${object}" bundled bundled "unset")
        get_json_element("${object}" find_args find_args "")
        get_json_element("${object}" raw_patches patches "")

        # okay here comes the fun part: REPLACEMENTS!
        # first: tag gets %VERSION% replaced if applicable,
        #   with version
        # second: artifact gets %VERSION% and %TAG% replaced
        #   accordingly (same rules for VERSION)

        # TODO(crueter): fmt module for cmake
        if(tag)
            string(REPLACE "%VERSION%" "${version}" tag ${tag})
        endif()

        if(artifact)
            string(REPLACE "%VERSION%" "${version}"
                artifact ${artifact})
            string(REPLACE "%TAG%" "${tag}" artifact ${artifact})
        endif()

        # format patchdir
        if(raw_patches)
            math(EXPR range "${raw_patches_LENGTH} - 1")

            foreach(IDX RANGE ${range})
                string(JSON _patch GET "${raw_patches}" "${IDX}")

                set(full_patch
                    "${CPMUTIL_PATCH_DIR}/${JSON_NAME}/${_patch}")
                if(NOT EXISTS ${full_patch})
                    cpm_utils_message(FATAL_ERROR ${JSON_NAME}
                        "specifies patch ${full_patch} which does not exist")
                endif()

                list(APPEND patches "${full_patch}")
            endforeach()
        endif()
        # end format patchdir

        # options
        get_json_element("${object}" raw_options options "")

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

    if(NOT DEFINED CPMFILE_CONTENT)
        cpm_utils_message(FATAL_ERROR ${name}
            "No cpmfile present")
        return()
    endif()

    if(NOT DEFINED JSON_NAME)
        cpm_utils_message(FATAL_ERROR "json package" "No name specified")
    endif()

    string(JSON object ERROR_VARIABLE
        err GET "${CPMFILE_CONTENT}" "${JSON_NAME}")

    if(err)
        cpm_utils_message(FATAL_ERROR ${JSON_NAME} "Not found in cpmfile")
    endif()

    parse_object(${object})

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
            MIN_VERSION "${min_version}"
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
            SHA "${sha}"
            REPO "${repo}"
            PATCHES "${patches}"
            OPTIONS "${options}"
            FIND_PACKAGE_ARGUMENTS "${find_args}"
            BUNDLED_PACKAGE "${bundled}"
            FORCE_BUNDLED_PACKAGE "${JSON_FORCE_BUNDLED_PACKAGE}"
            SOURCE_SUBDIR "${source_subdir}"

            GIT_HOST "${git_host}"

            ARTIFACT "${artifact}"
            TAG "${tag}"

            ${EXTRA_ARGS})
    endif()

    # pass stuff to parent scope
    Propagate(${package}_ADDED)
    Propagate(${package}_SOURCE_DIR)
    Propagate(${package}_BINARY_DIR)
    Propagate(CMAKE_PREFIX_PATH)
endfunction()

function(AddPackage)
    cpm_set_policies()
    set(EXTRA_ARGS "")

    set(oneValueArgs
        NAME
        VERSION
        MIN_VERSION
        GIT_HOST

        REPO
        TAG
        ARTIFACT
        SHA

        HASH

        URL

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
        cpm_utils_message(FATAL_ERROR "package" "No package name defined")
    endif()

    set(${PKG_ARGS_NAME}_CUSTOM_DIR "" CACHE STRING
        "Path to a separately-downloaded copy of ${PKG_ARGS_NAME}")
    option(${PKG_ARGS_NAME}_FORCE_SYSTEM
        "Force the system package for ${PKG_ARGS_NAME}")
    option(${PKG_ARGS_NAME}_FORCE_BUNDLED
        "Force the bundled package for ${PKG_ARGS_NAME}")

    if (DEFINED ${PKG_ARGS_NAME}_CUSTOM_DIR AND
        NOT ${PKG_ARGS_NAME}_CUSTOM_DIR STREQUAL "")
        set(CPM_${PKG_ARGS_NAME}_SOURCE ${${PKG_ARGS_NAME}_CUSTOM_DIR})
    endif()

    # TODO: See if this can be delegated to subshells
    if(NOT DEFINED PKG_ARGS_GIT_HOST)
        set(git_host github.com)
    else()
        set(git_host ${PKG_ARGS_GIT_HOST})
    endif()

    if(DEFINED PKG_ARGS_URL)
        set(pkg_url ${PKG_ARGS_URL})
        set(pkg_git_url ${pkg_url})
    elseif(DEFINED PKG_ARGS_REPO)
        set(pkg_git_url https://${git_host}/${PKG_ARGS_REPO})

        if(DEFINED PKG_ARGS_SHA)
            set(pkg_url "${pkg_git_url}/archive/${PKG_ARGS_SHA}.tar.gz")
        elseif(DEFINED PKG_ARGS_TAG)
            set(tag "${PKG_ARGS_TAG}")
            if(DEFINED PKG_ARGS_ARTIFACT)
                set(artifact "${PKG_ARGS_ARTIFACT}")
                set(pkg_url
                    "${pkg_git_url}/releases/download/${tag}/${artifact}")
            else()
                set(pkg_url
                    "${pkg_git_url}/archive/refs/tags/${tag}.tar.gz")
            endif()
        endif()
    else()
        cpm_utils_message(FATAL_ERROR ${PKG_ARGS_NAME}
            "No URL or repository defined")
    endif()

    cpm_utils_message(DEBUG ${PKG_ARGS_NAME} "Download URL is ${pkg_url}")

    if(DEFINED PKG_ARGS_SHA)
        string(SUBSTRING ${PKG_ARGS_SHA} 0 4 pkg_key)
    elseif(DEFINED PKG_ARGS_VERSION)
        set(pkg_key ${PKG_ARGS_VERSION})
    elseif(DEFINED PKG_ARGS_TAG)
        set(pkg_key ${PKG_ARGS_TAG})
    elseif(DEFINED PKG_ARGS_MIN_VERSION)
        set(pkg_key ${PKG_ARGS_MIN_VERSION})
    else()
        cpm_utils_message(FATAL_ERROR ${PKG_ARGS_NAME}
            "Could not determine cache key")
    endif()

    if(DEFINED PKG_ARGS_HASH)
        set(pkg_hash "SHA512=${PKG_ARGS_HASH}")
    else()
        cpm_utils_message(FATAL_ERROR ${PKG_ARGS_NAME}
            "No hash defined")
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
    ]]
    if(PKG_ARGS_FORCE_BUNDLED_PACKAGE)
        set_precedence(OFF OFF)
    elseif(${PKG_ARGS_NAME}_FORCE_SYSTEM)
        set_precedence(ON ON)
    elseif(${PKG_ARGS_NAME}_FORCE_BUNDLED)
        set_precedence(OFF OFF)
    elseif(CPMUTIL_FORCE_SYSTEM)
        set_precedence(ON ON)
    elseif(CPMUTIL_FORCE_BUNDLED)
        set_precedence(OFF OFF)
    elseif(DEFINED PKG_ARGS_BUNDLED_PACKAGE AND
        NOT PKG_ARGS_BUNDLED_PACKAGE STREQUAL "unset")
        if(PKG_ARGS_BUNDLED_PACKAGE)
            set(local OFF)
        else()
            set(local ON)
        endif()

        set_precedence(${local} OFF)
    else()
        set_precedence(ON OFF)
    endif()

    if(DEFINED PKG_ARGS_MIN_VERSION)
        list(APPEND EXTRA_ARGS
            VERSION ${PKG_ARGS_MIN_VERSION})
    endif()

    if (PKG_ARGS_FIND_PACKAGE_ARGUMENTS)
        list(APPEND EXTRA_ARGS
            FIND_PACKAGE_ARGUMENTS "${PKG_ARGS_FIND_PACKAGE_ARGUMENTS}")
    endif()

    if (PKG_ARGS_PATCHES)
        list(APPEND EXTRA_ARGS
            PATCHES "${PKG_ARGS_PATCHES}")
    endif()

    if (PKG_ARGS_OPTIONS)
        list(APPEND EXTRA_ARGS
            OPTIONS "${PKG_ARGS_OPTIONS}")
    endif()

    if (PKG_ARGS_SOURCE_SUBDIR)
        list(APPEND EXTRA_ARGS
            SOURCE_SUBDIR "${PKG_ARGS_SOURCE_SUBDIR}")
    endif()

    if (PKG_ARGS_DOWNLOAD_ONLY OR PKG_ARGS_MODULE_PATH)
        list(APPEND EXTRA_ARGS DOWNLOAD_ONLY ON)
    endif()

    CPMAddPackage(
        NAME ${PKG_ARGS_NAME}
        URL ${pkg_url}
        URL_HASH ${pkg_hash}
        CUSTOM_CACHE_KEY ${pkg_key}

        EXCLUDE_FROM_ALL ON

        ${EXTRA_ARGS}

        ${PKG_ARGS_UNPARSED_ARGUMENTS})

    set_property(GLOBAL APPEND PROPERTY CPM_PACKAGE_NAMES ${PKG_ARGS_NAME})
    set_property(GLOBAL APPEND PROPERTY CPM_PACKAGE_URLS ${pkg_git_url})

    if(${PKG_ARGS_NAME}_ADDED)
        if(DEFINED PKG_ARGS_SHA)
            set_property(GLOBAL APPEND PROPERTY CPM_PACKAGE_SHAS
                ${PKG_ARGS_SHA})
        elseif(DEFINED PKG_ARGS_VERSION)
            set_property(GLOBAL APPEND PROPERTY CPM_PACKAGE_SHAS
                ${PKG_ARGS_VERSION})
        elseif(DEFINED PKG_ARGS_TAG)
            set_property(GLOBAL APPEND PROPERTY CPM_PACKAGE_SHAS
                ${PKG_ARGS_TAG})
        elseif(DEFINED PKG_ARGS_MIN_VERSION)
            set_property(GLOBAL APPEND PROPERTY CPM_PACKAGE_SHAS
                ${PKG_ARGS_MIN_VERSION})
        else()
            cpm_utils_message(WARNING ${PKG_ARGS_NAME}
                "Package has no specified sha, tag, or version")
            set_property(GLOBAL APPEND PROPERTY CPM_PACKAGE_SHAS "unknown")
        endif()
    else()
        if(DEFINED CPM_PACKAGE_${PKG_ARGS_NAME}_VERSION AND NOT
            "${CPM_PACKAGE_${PKG_ARGS_NAME}_VERSION}" STREQUAL "")
            set_property(GLOBAL APPEND PROPERTY CPM_PACKAGE_SHAS
                "${CPM_PACKAGE_${PKG_ARGS_NAME}_VERSION} (system)")
        else()
            set_property(GLOBAL APPEND PROPERTY CPM_PACKAGE_SHAS
                "unknown (system)")
        endif()
    endif()

    # pass stuff to parent scope
    Propagate(${PKG_ARGS_NAME}_ADDED)
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

    # TODO: use cpm_utils_message
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

    if(NOT DEFINED PKG_ARGS_CMAKE_FILENAME)
        set(ARTIFACT_CMAKE ${PKG_ARGS_NAME})
    else()
        set(ARTIFACT_CMAKE ${PKG_ARGS_CMAKE_FILENAME})
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

    if(DEFINED PKG_ARGS_MIN_VERSION)
        set(ARTIFACT_MIN_VERSION ${PKG_ARGS_MIN_VERSION})
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
        cpm_utils_message(WARNING ${PKG_ARGS_NAME}
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
        cpm_utils_message(WARNING ${PKG_ARGS_NAME}
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
        # TODO:
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
            TAG "v${ARTIFACT_VERSION}"
            MIN_VERSION ${ARTIFACT_VERSION}
            ARTIFACT ${ARTIFACT}
            HASH ${sha512sum_hash}
            FORCE_BUNDLED_PACKAGE ON
            ${EXTRA_ARGS})

        set(${ARTIFACT_PACKAGE}_ADDED TRUE PARENT_SCOPE)
        Propagate(${ARTIFACT_PACKAGE}_SOURCE_DIR)
        Propagate(CMAKE_PREFIX_PATH)
    else()
        find_package(${ARTIFACT_PACKAGE} ${ARTIFACT_MIN_VERSION} REQUIRED)
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
        MIN_VERSION 6
        REPO ${repo}
        DISABLED_PLATFORMS
            android-x86_64 android-aarch64
        MODULE_PATH)

    find_package(Qt6 REQUIRED PATHS ${Qt6_SOURCE_DIR} NO_DEFAULT_PATH)

    Propagate(CMAKE_PREFIX_PATH)
    Propagate(Qt6_SOURCE_DIR)
endfunction()
