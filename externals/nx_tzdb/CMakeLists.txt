# SPDX-FileCopyrightText: 2023 yuzu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

set(NX_TZDB_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/include")

add_library(nx_tzdb INTERFACE)

find_program(GIT git)
find_program(GNU_MAKE make)
find_program(GNU_DATE date)

set(CAN_BUILD_NX_TZDB true)

if ("${GIT}" STREQUAL "GIT-NOTFOUND")
    message("git program not found")
    set(CAN_BUILD_NX_TZDB false)
endif()
if ("${GNU_MAKE}" STREQUAL "GNU_MAKE-NOTFOUND")
    message("GNU make program not found")
    set(CAN_BUILD_NX_TZDB false)
endif()
if ("${GNU_DATE}" STREQUAL "GNU_DATE-NOTFOUND")
    message("GNU date program not found")
    set(CAN_BUILD_NX_TZDB false)
endif()

set(NX_TZDB_VERSION "220816")
set(NX_TZDB_ARCHIVE "${CMAKE_CURRENT_BINARY_DIR}/${NX_TZDB_VERSION}.zip")

if ((NOT ${CAN_BUILD_NX_TZDB} OR ${YUZU_DOWNLOAD_TIME_ZONE_DATA}) AND NOT EXISTS ${NX_TZDB_ARCHIVE})
    set(NX_TZDB_DOWNLOAD_URL "https://github.com/lat9nq/tzdb_to_nx/releases/download/${NX_TZDB_VERSION}/${NX_TZDB_VERSION}.zip")

    set(NX_TZDB_DIR "${CMAKE_CURRENT_BINARY_DIR}/nx_tzdb" CACHE PATH "Path to Switch-style time zone data")

    file(DOWNLOAD ${NX_TZDB_DOWNLOAD_URL} ${NX_TZDB_ARCHIVE})
    file(ARCHIVE_EXTRACT
        INPUT
            ${NX_TZDB_ARCHIVE}
        DESTINATION
            ${NX_TZDB_DIR})
elseif (${CAN_BUILD_NX_TZDB})
    add_subdirectory(tzdb_to_nx)
    add_dependencies(nx_tzdb x80e)
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

CreateHeader(${NX_TZDB_DIR} base)
CreateHeader(${NX_TZDB_DIR}/zoneinfo zoneinfo)
CreateHeader(${NX_TZDB_DIR}/zoneinfo/Africa africa)
CreateHeader(${NX_TZDB_DIR}/zoneinfo/America america)
CreateHeader(${NX_TZDB_DIR}/zoneinfo/America/Argentina america_argentina)
CreateHeader(${NX_TZDB_DIR}/zoneinfo/America/Indiana america_indiana)
CreateHeader(${NX_TZDB_DIR}/zoneinfo/America/Kentucky america_kentucky)
CreateHeader(${NX_TZDB_DIR}/zoneinfo/America/North_Dakota america_north_dakota)
CreateHeader(${NX_TZDB_DIR}/zoneinfo/Antarctica antarctica)
CreateHeader(${NX_TZDB_DIR}/zoneinfo/Arctic arctic)
CreateHeader(${NX_TZDB_DIR}/zoneinfo/Asia asia)
CreateHeader(${NX_TZDB_DIR}/zoneinfo/Atlantic atlantic)
CreateHeader(${NX_TZDB_DIR}/zoneinfo/Australia australia)
CreateHeader(${NX_TZDB_DIR}/zoneinfo/Brazil brazil)
CreateHeader(${NX_TZDB_DIR}/zoneinfo/Canada canada)
CreateHeader(${NX_TZDB_DIR}/zoneinfo/Chile chile)
CreateHeader(${NX_TZDB_DIR}/zoneinfo/Etc etc)
CreateHeader(${NX_TZDB_DIR}/zoneinfo/Europe europe)
CreateHeader(${NX_TZDB_DIR}/zoneinfo/Indian indian)
CreateHeader(${NX_TZDB_DIR}/zoneinfo/Mexico mexico)
CreateHeader(${NX_TZDB_DIR}/zoneinfo/Pacific pacific)
CreateHeader(${NX_TZDB_DIR}/zoneinfo/US us)
