# SPDX-FileCopyrightText: 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

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

if (NOT (GIT AND GNU_MAKE AND DATE_PROG) OR CMAKE_SYSTEM_NAME STREQUAL "Windows" OR ANDROID)
    # tzdb_to_nx currently requires a posix-compliant host
    # MinGW and Android are handled here due to the executable format being different from the host system
    # TODO (lat9nq): cross-compiling support

    set(CAN_BUILD_NX_TZDB false)
endif()

if (CAN_BUILD_NX_TZDB AND NOT YUZU_DOWNLOAD_TIME_ZONE_DATA)
    message(FATAL_ERROR "Building tzdb is currently unsupported. Check back later.")
    add_subdirectory(tzdb_to_nx)
    add_dependencies(nx_tzdb x80e)

    set(NX_TZDB_BASE_DIR "${NX_TZDB_DIR}")
    set(NX_TZDB_TZ_DIR "${NX_TZDB_BASE_DIR}/zoneinfo")
endif()

# TODO(crueter): This is a terrible solution, but MSVC fails to link without it
# Need to investigate further but I still can't reproduce...
if (MSVC)
    set(NX_TZDB_VERSION "250725")
    set(NX_TZDB_ARCHIVE "${CPM_SOURCE_CACHE}/nx_tzdb/${NX_TZDB_VERSION}.zip")

    set(NX_TZDB_BASE_DIR "${CPM_SOURCE_CACHE}/nx_tzdb/tz")
    set(NX_TZDB_TZ_DIR "${NX_TZDB_BASE_DIR}/zoneinfo")

    set(NX_TZDB_DOWNLOAD_URL "https://github.com/crueter/tzdb_to_nx/releases/download/${NX_TZDB_VERSION}/${NX_TZDB_VERSION}.zip")

    message(STATUS "Downloading time zone data from ${NX_TZDB_DOWNLOAD_URL}...")
    file(DOWNLOAD ${NX_TZDB_DOWNLOAD_URL} ${NX_TZDB_ARCHIVE}
        STATUS NX_TZDB_DOWNLOAD_STATUS)

    list(GET NX_TZDB_DOWNLOAD_STATUS 0 NX_TZDB_DOWNLOAD_STATUS_CODE)
    if (NOT NX_TZDB_DOWNLOAD_STATUS_CODE EQUAL 0)
        message(FATAL_ERROR "Time zone data download failed (status code ${NX_TZDB_DOWNLOAD_STATUS_CODE})")
    endif()

    file(ARCHIVE_EXTRACT
        INPUT
            ${NX_TZDB_ARCHIVE}
        DESTINATION
            ${NX_TZDB_BASE_DIR})
else()
    message(STATUS "Downloading time zone data...")
    AddJsonPackage(tzdb)

    target_include_directories(nx_tzdb
        INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include
        INTERFACE ${NX_TZDB_INCLUDE_DIR})

    set(NX_TZDB_BASE_DIR "${CPM_SOURCE_CACHE}/nx_tzdb")
    set(NX_TZDB_TZ_DIR "${nx_tzdb_SOURCE_DIR}")
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

CreateHeader(${NX_TZDB_BASE_DIR} base)
CreateHeader(${NX_TZDB_TZ_DIR} zoneinfo)
CreateHeader(${NX_TZDB_TZ_DIR}/Africa africa)
CreateHeader(${NX_TZDB_TZ_DIR}/America america)
CreateHeader(${NX_TZDB_TZ_DIR}/America/Argentina america_argentina)
CreateHeader(${NX_TZDB_TZ_DIR}/America/Indiana america_indiana)
CreateHeader(${NX_TZDB_TZ_DIR}/America/Kentucky america_kentucky)
CreateHeader(${NX_TZDB_TZ_DIR}/America/North_Dakota america_north_dakota)
CreateHeader(${NX_TZDB_TZ_DIR}/Antarctica antarctica)
CreateHeader(${NX_TZDB_TZ_DIR}/Arctic arctic)
CreateHeader(${NX_TZDB_TZ_DIR}/Asia asia)
CreateHeader(${NX_TZDB_TZ_DIR}/Atlantic atlantic)
CreateHeader(${NX_TZDB_TZ_DIR}/Australia australia)
CreateHeader(${NX_TZDB_TZ_DIR}/Brazil brazil)
CreateHeader(${NX_TZDB_TZ_DIR}/Canada canada)
CreateHeader(${NX_TZDB_TZ_DIR}/Chile chile)
CreateHeader(${NX_TZDB_TZ_DIR}/Etc etc)
CreateHeader(${NX_TZDB_TZ_DIR}/Europe europe)
CreateHeader(${NX_TZDB_TZ_DIR}/Indian indian)
CreateHeader(${NX_TZDB_TZ_DIR}/Mexico mexico)
CreateHeader(${NX_TZDB_TZ_DIR}/Pacific pacific)
CreateHeader(${NX_TZDB_TZ_DIR}/US us)
