# SPDX-FileCopyrightText: 2023 yuzu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

# Explicitly include CPMUtil here since we have a separate cpmfile for nx_tzdb
set(CPMUTIL_JSON_FILE ${CMAKE_CURRENT_SOURCE_DIR}/cpmfile.json)
include(CPMUtil)

set(NX_TZDB_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/include")

add_library(nx_tzdb INTERFACE)

find_program(GIT git)
find_program(GNU_MAKE make)
find_program(DATE_PROG date)

set(CAN_BUILD_NX_TZDB true)

if (NOT GIT)
    set(CAN_BUILD_NX_TZDB false)
endif()
if (NOT GNU_MAKE)
    set(CAN_BUILD_NX_TZDB false)
endif()
if (NOT DATE_PROG)
    set(CAN_BUILD_NX_TZDB false)
endif()
if (CMAKE_SYSTEM_NAME STREQUAL "Windows" OR ANDROID)
    # tzdb_to_nx currently requires a posix-compliant host
    # MinGW and Android are handled here due to the executable format being different from the host system
    # TODO (lat9nq): cross-compiling support
    set(CAN_BUILD_NX_TZDB false)
endif()

set(NX_TZDB_VERSION "250725")

set(NX_TZDB_ROMFS_DIR "${CPM_SOURCE_CACHE}/nx_tzdb/")

if ((NOT CAN_BUILD_NX_TZDB OR YUZU_DOWNLOAD_TIME_ZONE_DATA) AND NOT EXISTS ${NX_TZDB_ROMFS_DIR})
    message(STATUS "Downloading time zone data...")
    AddJsonPackage(tzdb)
elseif (CAN_BUILD_NX_TZDB AND NOT YUZU_DOWNLOAD_TIME_ZONE_DATA)
    # TODO(crueter): this sucked to do with cpm, see if i can get it to work again
    message(FATAL_ERROR "Building tzdb is currently unsupported. Check back later.")
    add_subdirectory(tzdb_to_nx)
    add_dependencies(nx_tzdb x80e)

    set(NX_TZDB_ROMFS_DIR "${NX_TZDB_DIR}")
endif()

target_include_directories(nx_tzdb
    INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include
    INTERFACE ${NX_TZDB_INCLUDE_DIR})

function(CreateHeader ZONE_PATH HEADER_NAME)
    set(HEADER_PATH "${NX_TZDB_INCLUDE_DIR}/nx_tzdb/${HEADER_NAME}.h")
    add_custom_command(
        OUTPUT
            ${NX_TZDB_INCLUDE_DIR}/nx_tzdb/${HEADER_NAME}.h
        COMMAND
            ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/NxTzdbCreateHeader.cmake
                ${ZONE_PATH}
                ${HEADER_NAME}
                ${NX_TZDB_INCLUDE_DIR}
                ${CMAKE_CURRENT_SOURCE_DIR}
        DEPENDS
            tzdb_template.h.in
            NxTzdbCreateHeader.cmake)

    target_sources(nx_tzdb PRIVATE ${HEADER_PATH})
endfunction()

CreateHeader(${NX_TZDB_ROMFS_DIR} base)
CreateHeader(${NX_TZDB_ROMFS_DIR}/${NX_TZDB_VERSION} zoneinfo)
CreateHeader(${NX_TZDB_ROMFS_DIR}/${NX_TZDB_VERSION}/Africa africa)
CreateHeader(${NX_TZDB_ROMFS_DIR}/${NX_TZDB_VERSION}/America america)
CreateHeader(${NX_TZDB_ROMFS_DIR}/${NX_TZDB_VERSION}/America/Argentina america_argentina)
CreateHeader(${NX_TZDB_ROMFS_DIR}/${NX_TZDB_VERSION}/America/Indiana america_indiana)
CreateHeader(${NX_TZDB_ROMFS_DIR}/${NX_TZDB_VERSION}/America/Kentucky america_kentucky)
CreateHeader(${NX_TZDB_ROMFS_DIR}/${NX_TZDB_VERSION}/America/North_Dakota america_north_dakota)
CreateHeader(${NX_TZDB_ROMFS_DIR}/${NX_TZDB_VERSION}/Antarctica antarctica)
CreateHeader(${NX_TZDB_ROMFS_DIR}/${NX_TZDB_VERSION}/Arctic arctic)
CreateHeader(${NX_TZDB_ROMFS_DIR}/${NX_TZDB_VERSION}/Asia asia)
CreateHeader(${NX_TZDB_ROMFS_DIR}/${NX_TZDB_VERSION}/Atlantic atlantic)
CreateHeader(${NX_TZDB_ROMFS_DIR}/${NX_TZDB_VERSION}/Australia australia)
CreateHeader(${NX_TZDB_ROMFS_DIR}/${NX_TZDB_VERSION}/Brazil brazil)
CreateHeader(${NX_TZDB_ROMFS_DIR}/${NX_TZDB_VERSION}/Canada canada)
CreateHeader(${NX_TZDB_ROMFS_DIR}/${NX_TZDB_VERSION}/Chile chile)
CreateHeader(${NX_TZDB_ROMFS_DIR}/${NX_TZDB_VERSION}/Etc etc)
CreateHeader(${NX_TZDB_ROMFS_DIR}/${NX_TZDB_VERSION}/Europe europe)
CreateHeader(${NX_TZDB_ROMFS_DIR}/${NX_TZDB_VERSION}/Indian indian)
CreateHeader(${NX_TZDB_ROMFS_DIR}/${NX_TZDB_VERSION}/Mexico mexico)
CreateHeader(${NX_TZDB_ROMFS_DIR}/${NX_TZDB_VERSION}/Pacific pacific)
CreateHeader(${NX_TZDB_ROMFS_DIR}/${NX_TZDB_VERSION}/US us)
