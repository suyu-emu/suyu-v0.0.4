find_program(GIT_PROGRAM git)
if (NOT GIT_PROGRAM)
    message(FATAL_ERROR "git program not found")
endif()

find_program(GNU_DATE date)
if (NOT GNU_DATE)
    message(FATAL_ERROR "date program not found")
endif()

set(NX_TZDB_DIR "${CMAKE_CURRENT_BINARY_DIR}/nx" CACHE PATH "Path to Switch-style time zone data")
set(NX_ZONEINFO_DIR "${NX_TZDB_DIR}/zoneinfo")

set(TZDB_VERSION_FILE ${TZ_SOURCE_DIR}/NEWS)

if (NOT "${TZDB2NX_VERSION}" STREQUAL "")
    set(TZDB_VERSION "${TZDB2NX_VERSION}\n")
else()
    execute_process(
        COMMAND 
            ${GIT_PROGRAM} log --pretty=%at -n1 NEWS
        OUTPUT_VARIABLE
            TZ_COMMIT_TIME
        WORKING_DIRECTORY
            ${TZ_SOURCE_DIR}
        COMMAND_ERROR_IS_FATAL ANY)

    string(REPLACE "\n" "" TZ_COMMIT_TIME "${TZ_COMMIT_TIME}")

    if (APPLE OR CMAKE_SYSTEM_NAME MATCHES "DragonFly|FreeBSD|NetBSD|OpenBSD")
        set(VERSION_COMMAND ${GNU_DATE} -r ${TZ_COMMIT_TIME} +%y%m%d)
    else ()
        set(VERSION_COMMAND ${GNU_DATE} +%y%m%d --date=@${TZ_COMMIT_TIME})
    endif ()

    execute_process(
        COMMAND
            ${VERSION_COMMAND}
        OUTPUT_VARIABLE
            TZDB_VERSION
        COMMAND_ERROR_IS_FATAL ANY)
endif()

set(NX_VERSION_FILE ${NX_TZDB_DIR}/version.txt)
file(WRITE ${NX_VERSION_FILE} "${TZDB_VERSION}")

add_custom_target(x80e
    DEPENDS
        tzdb2nx
        ${NX_VERSION_FILE})

set(BINARY_LIST_TXT ${NX_TZDB_DIR}/binaryList.txt)
add_custom_command(
    OUTPUT
        ${BINARY_LIST_TXT}
    COMMAND
        ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/generate_binary_list_txt.cmake ${BINARY_LIST_TXT} ${PROJECT_SOURCE_DIR}/CMakeModules/list_directory.cmake
    WORKING_DIRECTORY
        ${NX_ZONEINFO_DIR})

add_custom_target(time_zone_binary_list
    DEPENDS ${BINARY_LIST_TXT})
add_dependencies(x80e time_zone_binary_list)

set(TZ_DATA_LIST "")

file(STRINGS "${TZIF_LIST_FILE}" TZ_FILES)
foreach(FILE ${TZ_FILES})
    file(RELATIVE_PATH TARG "${TZ_ZONEINFO_DIR}" "${FILE}")
    get_filename_component(TARG_PATH "${NX_ZONEINFO_DIR}/${TARG}" DIRECTORY)
    string(REGEX REPLACE "\/" "_" TARG_SANITIZED "${TARG}")
    set(NX_TZ_TARGET ${NX_ZONEINFO_DIR}/${TARG})
    add_custom_command(
        OUTPUT
            ${NX_TZ_TARGET}
        COMMAND
            mkdir -p ${TARG_PATH}
        COMMAND
            ${TZDB2NX_PATH} ${FILE} ${NX_ZONEINFO_DIR}/${TARG}
        DEPENDS
            tzdb2nx)

    list(APPEND TZ_DATA_LIST ${NX_TZ_TARGET})
endforeach()

add_custom_target(time_zone_data
    DEPENDS ${TZ_DATA_LIST})

add_dependencies(x80e time_zone_data)
add_dependencies(time_zone_binary_list time_zone_data)
