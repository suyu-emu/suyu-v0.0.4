# SPDX-FileCopyrightText: Copyright 2026 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

# CPM.cmake - CMake's missing package manager
# ===========================================
# See https://github.com/cpm-cmake/CPM.cmake for usage and update instructions.
#
# MIT License
# -----------
#[[
  Copyright (c) 2019-2023 Lars Melchior and contributors

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
]]

cmake_minimum_required(VERSION 3.31 FATAL_ERROR)

# Initialize logging prefix
if(NOT CPM_INDENT)
  set(CPM_INDENT "CPM:" CACHE INTERNAL "")
endif()

get_property(CPM_INITIALIZED GLOBAL PROPERTY CPM_INITIALIZED)
if(CPM_INITIALIZED)
  return()
endif()

set_property(GLOBAL PROPERTY CPM_INITIALIZED TRUE)

macro(cpm_set_policies)
  # the policy allows us to change options without caching
  cmake_policy(SET CMP0077 NEW)
  set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

  # the policy allows us to change set(CACHE) without caching
  cmake_policy(SET CMP0126 NEW)
  set(CMAKE_POLICY_DEFAULT_CMP0126 NEW)

  # The policy uses the download time for timestamp,
  # instead of the timestamp in the archive. This
  # allows for proper rebuilds when a projects url changes
  cmake_policy(SET CMP0135 NEW)
  set(CMAKE_POLICY_DEFAULT_CMP0135 NEW)

  # treat relative git repository paths as
  # being relative to the parent project's remote
  cmake_policy(SET CMP0150 NEW)
  set(CMAKE_POLICY_DEFAULT_CMP0150 NEW)

  # Disable sub-build nonsense
  cmake_policy(SET CMP0168 NEW)
  set(CMAKE_POLICY_DEFAULT_CMP0168 NEW)
endmacro()

cpm_set_policies()

option(CPM_DONT_UPDATE_MODULE_PATH
  "Don't update the module path to allow using find_package"
  $ENV{CPM_DONT_UPDATE_MODULE_PATH})

set(CPM_FILE
    ${CMAKE_CURRENT_LIST_FILE}
    CACHE INTERNAL "")

set(CPM_PACKAGES "" CACHE INTERNAL "")

if(NOT CPM_DONT_UPDATE_MODULE_PATH
   AND NOT DEFINED CMAKE_FIND_PACKAGE_REDIRECTS_DIR)
  set(CPM_MODULE_PATH "${CMAKE_BINARY_DIR}/CPM_modules" CACHE INTERNAL "")
  # remove old modules
  file(REMOVE_RECURSE ${CPM_MODULE_PATH})
  file(MAKE_DIRECTORY ${CPM_MODULE_PATH})
  # locally added CPM modules should override global packages
  set(CMAKE_MODULE_PATH "${CPM_MODULE_PATH};${CMAKE_MODULE_PATH}")
endif()

include(FetchContent)

# Create a custom FindXXX.cmake module for a CPM package.
# This prevents `find_package(NAME)` from finding the system library
function(cpm_create_module_file Name)
  if(NOT CPM_DONT_UPDATE_MODULE_PATH)
    if(DEFINED CMAKE_FIND_PACKAGE_REDIRECTS_DIR)
      # Redirect find_package calls to the CPM package.
      # This is what FetchContent does when you set
      # OVERRIDE_FIND_PACKAGE. The CMAKE_FIND_PACKAGE_REDIRECTS_DIR works for
      # find_package in CONFIG mode, unlike the Find${Name}.cmake fallback.
      # CMAKE_FIND_PACKAGE_REDIRECTS_DIR is not defined in script mode
      # https://cmake.org/cmake/help/latest/module/FetchContent.html
      string(TOLOWER ${Name} NameLower)
      file(WRITE ${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/${NameLower}-config.cmake
        "include(\"\${CMAKE_CURRENT_LIST_DIR}/${NameLower}-extra.cmake\" \
          OPTIONAL)\n"
        "include(\"\${CMAKE_CURRENT_LIST_DIR}/${Name}Extra.cmake\" OPTIONAL)\n")

      file(WRITE
        ${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/${NameLower}-config-version.cmake
        "set(PACKAGE_VERSION_COMPATIBLE TRUE)\n"
        "set(PACKAGE_VERSION_EXACT TRUE)\n")
    else()
      file(WRITE ${CPM_MODULE_PATH}/Find${Name}.cmake
      "include(\"${CPM_FILE}\")\n${ARGN}\nset(${Name}_FOUND TRUE)")
    endif()
  endif()
endfunction()

# checks if a package has been added before
function(cpm_check_if_package_already_added CPM_ARGS_NAME CPM_ARGS_VERSION)
  if("${CPM_ARGS_NAME}" IN_LIST CPM_PACKAGES)
    CPMGetPackageVersion(${CPM_ARGS_NAME} CPM_PACKAGE_VERSION)
    if("${CPM_PACKAGE_VERSION}" VERSION_LESS "${CPM_ARGS_VERSION}")
      message(WARNING
        "${CPM_INDENT} Requires a newer version of ${CPM_ARGS_NAME} "
          "(${CPM_ARGS_VERSION}) than currently included "
          "(${CPM_PACKAGE_VERSION}).")
    endif()
    cpm_get_fetch_properties(${CPM_ARGS_NAME})
    set(${CPM_ARGS_NAME}_ADDED NO)
    set(CPM_PACKAGE_ALREADY_ADDED YES PARENT_SCOPE)
    cpm_export_variables(${CPM_ARGS_NAME})
  else()
    set(CPM_PACKAGE_ALREADY_ADDED NO PARENT_SCOPE)
  endif()
endfunction()

function(CPMAddPackage)
  set(oneValueArgs
      NAME
      FORCE
      VERSION
      DOWNLOAD_ONLY
      SOURCE_DIR
      SYSTEM
      EXCLUDE_FROM_ALL
      SOURCE_SUBDIR)

  set(multiValueArgs OPTIONS)

  cmake_parse_arguments(CPM_ARGS
    ""
    "${oneValueArgs}"
    "${multiValueArgs}"
    "${ARGN}")

  if (NOT DEFINED CPM_ARGS_SOURCE_DIR)
    message(FATAL_ERROR "${CPM_INDENT} SOURCE_DIR is required")
  endif()

  if(CPM_ARGS_DOWNLOAD_ONLY)
    set(DOWNLOAD_ONLY ${CPM_ARGS_DOWNLOAD_ONLY})
  else()
    # Default to DOWNLOAD_ONLY in script mode.
    set(DOWNLOAD_ONLY ${CMAKE_SCRIPT_MODE_FILE})
  endif()

  if(NOT DEFINED CPM_ARGS_NAME)
    message(FATAL_ERROR "${CPM_INDENT} NAME is required")
  endif()

  if(NOT DEFINED CPM_ARGS_VERSION)
    message(FATAL_ERROR "${CPM_INDENT} VERSION is required")
  endif()

  cpm_check_if_package_already_added(${CPM_ARGS_NAME} "${CPM_ARGS_VERSION}")
  if(CPM_PACKAGE_ALREADY_ADDED)
    cpm_export_variables(${CPM_ARGS_NAME})
    return()
  endif()

  if(NOT CPM_ARGS_FORCE AND NOT "${CPM_${CPM_ARGS_NAME}_SOURCE}" STREQUAL "")
    set(PACKAGE_SOURCE ${CPM_${CPM_ARGS_NAME}_SOURCE})
    set(CPM_${CPM_ARGS_NAME}_SOURCE "")
    CPMAddPackage(
      NAME "${CPM_ARGS_NAME}"
      VERSION "${CPM_ARGS_VERSION}"
      SOURCE_DIR "${PACKAGE_SOURCE}"
      EXCLUDE_FROM_ALL "${CPM_ARGS_EXCLUDE_FROM_ALL}"
      SYSTEM "${CPM_ARGS_SYSTEM}"
      PATCHES "${CPM_ARGS_PATCHES}"
      OPTIONS "${CPM_ARGS_OPTIONS}"
      SOURCE_SUBDIR "${CPM_ARGS_SOURCE_SUBDIR}"
      DOWNLOAD_ONLY "${DOWNLOAD_ONLY}"
      FORCE True)

    cpm_export_variables(${CPM_ARGS_NAME})
    return()
  endif()

  CPMRegisterPackage("${CPM_ARGS_NAME}" "${CPM_ARGS_VERSION}")

  if(DEFINED FETCHCONTENT_BASE_DIR)
    set(CPM_FETCHCONTENT_BASE_DIR ${FETCHCONTENT_BASE_DIR})
  else()
    set(CPM_FETCHCONTENT_BASE_DIR ${CMAKE_BINARY_DIR}/_deps)
  endif()

  # Subbuild, etc
  string(TOLOWER ${CPM_ARGS_NAME} lower_case_name)

  set(fetch_source_dir ${CPM_ARGS_SOURCE_DIR})
  if(NOT IS_ABSOLUTE ${CPM_ARGS_SOURCE_DIR})
    get_filename_component(
      fetch_source_dir ${CPM_ARGS_SOURCE_DIR}
      REALPATH BASE_DIR ${CMAKE_CURRENT_BINARY_DIR})
  endif()

  # FetchContent args
  set(fetchContentDeclareExtraArgs "")
  if(${CPM_ARGS_EXCLUDE_FROM_ALL})
    list(APPEND fetchContentDeclareExtraArgs EXCLUDE_FROM_ALL)
  endif()

  if(CPM_ARGS_SYSTEM)
    list(APPEND fetchContentDeclareExtraArgs SYSTEM)
  endif()

  if(DEFINED CPM_ARGS_SOURCE_SUBDIR)
    list(APPEND fetchContentDeclareExtraArgs
      SOURCE_SUBDIR ${CPM_ARGS_SOURCE_SUBDIR})
  endif()

  if(CPM_ARGS_OPTIONS AND NOT DOWNLOAD_ONLY)
    foreach(OPTION ${CPM_ARGS_OPTIONS})
      cpm_parse_option("${OPTION}")
      set(${OPTION_KEY} "${OPTION_VALUE}")
    endforeach()
  endif()

  if(NOT DOWNLOAD_ONLY)
    cpm_create_module_file(${CPM_ARGS_NAME} "CPMAddPackage(\"${ARGN}\")")
  endif()

  FetchContent_Declare(${CPM_ARGS_NAME}
    ${fetchContentDeclareExtraArgs}
    SOURCE_DIR ${fetch_source_dir}
    DOWNLOAD_NO_PROGRESS TRUE)

  if(DOWNLOAD_ONLY)
    FetchContent_Populate(${CPM_ARGS_NAME}
      ${fetchContentDeclareExtraArgs}
      SOURCE_DIR "${fetch_source_dir}"
      BINARY_DIR "${CPM_FETCHCONTENT_BASE_DIR}/${lower_case_name}-build"
      SUBBUILD_DIR "${CPM_FETCHCONTENT_BASE_DIR}/${lower_case_name}-subbuild"
      DOWNLOAD_NO_PROGRESS TRUE)
  else()
    FetchContent_MakeAvailable(${CPM_ARGS_NAME})
  endif()

  FetchContent_GetProperties(${CPM_ARGS_NAME})
  cpm_store_fetch_properties(
    ${CPM_ARGS_NAME}
    ${${lower_case_name}_SOURCE_DIR}
    ${${lower_case_name}_BINARY_DIR})

  cpm_get_fetch_properties("${CPM_ARGS_NAME}")

  set(${CPM_ARGS_NAME}_ADDED YES)
  cpm_export_variables("${CPM_ARGS_NAME}")
endfunction()

# export variables available to the caller to the
# parent scope expects ${CPM_ARGS_NAME} to be set
macro(cpm_export_variables name)
  set(${name}_SOURCE_DIR "${${name}_SOURCE_DIR}" PARENT_SCOPE)
  set(${name}_BINARY_DIR "${${name}_BINARY_DIR}" PARENT_SCOPE)
  set(${name}_ADDED "${${name}_ADDED}" PARENT_SCOPE)
  set(CPM_LAST_PACKAGE_NAME "${name}" PARENT_SCOPE)
endmacro()

# registers a package that has been added to CPM
function(CPMRegisterPackage PACKAGE VERSION)
  list(APPEND CPM_PACKAGES ${PACKAGE})
  set(CPM_PACKAGES ${CPM_PACKAGES} CACHE INTERNAL "")
  set("CPM_PACKAGE_${PACKAGE}_VERSION" ${VERSION}CACHE INTERNAL "")
endfunction()

# retrieve the current version of the package to ${OUTPUT}
function(CPMGetPackageVersion PACKAGE OUTPUT)
  set(${OUTPUT} "${CPM_PACKAGE_${PACKAGE}_VERSION}" PARENT_SCOPE)
endfunction()

function(cpm_get_fetch_properties PACKAGE)
  set(${PACKAGE}_SOURCE_DIR "${CPM_PACKAGE_${PACKAGE}_SOURCE_DIR}"
    PARENT_SCOPE)

  set(${PACKAGE}_BINARY_DIR "${CPM_PACKAGE_${PACKAGE}_BINARY_DIR}"
      PARENT_SCOPE)
endfunction()

function(cpm_store_fetch_properties PACKAGE source_dir binary_dir)
  set(CPM_PACKAGE_${PACKAGE}_SOURCE_DIR "${source_dir}" CACHE INTERNAL "")
  set(CPM_PACKAGE_${PACKAGE}_BINARY_DIR "${binary_dir}" CACHE INTERNAL "")
endfunction()

# splits a package option
function(cpm_parse_option OPTION)
  string(REGEX MATCH "^[^ ]+" OPTION_KEY "${OPTION}")
  string(LENGTH "${OPTION}" OPTION_LENGTH)
  string(LENGTH "${OPTION_KEY}" OPTION_KEY_LENGTH)

  if(OPTION_KEY_LENGTH STREQUAL OPTION_LENGTH)
    # no value for key provided, assume user wants to set option to "ON"
    set(OPTION_VALUE "ON")
  else()
    math(EXPR OPTION_KEY_LENGTH "${OPTION_KEY_LENGTH}+1")
    string(SUBSTRING "${OPTION}" "${OPTION_KEY_LENGTH}" "-1" OPTION_VALUE)
  endif()

  set(OPTION_KEY "${OPTION_KEY}" PARENT_SCOPE)
  set(OPTION_VALUE "${OPTION_VALUE}" PARENT_SCOPE)
endfunction()
