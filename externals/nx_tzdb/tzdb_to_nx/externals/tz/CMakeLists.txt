set(TZ_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tz" CACHE PATH "Time zone source directory")
set(TZ_DIR "${CMAKE_CURRENT_BINARY_DIR}/tz")
set(TZ_TMP_SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/tmpsrc")
set(TZIF_LIST_FILE "${CMAKE_CURRENT_BINARY_DIR}/tzif_list.txt" CACHE PATH "List of zone info files")
if (TZDB2NX_ZONEINFO_DIR)
    set(TZ_ZONEINFO_DIR "${TZDB2NX_ZONEINFO_DIR}" CACHE PATH "Time zone info data directory")
else()
    set(TZ_ZONEINFO_DIR "${TZ_DIR}/usr/share/zoneinfo" CACHE PATH "Time zone info data directory")
endif()

find_program(GNU_MAKE make)
if (NOT GNU_MAKE)
    message(FATAL_ERROR "GNU make not found")
endif()

find_program(GIT_PROGRAM git)
if (NOT GIT_PROGRAM)
    message(FATAL_ERROR "git program not found")
endif()

if (NOT EXISTS "${TZ_DIR}" OR NOT EXISTS "${TZIF_LIST_FILE}")
    if (NOT TZDB2NX_ZONEINFO_DIR) # If a custom zoneinfo directory was specified
        # tz's makefile can only build in-tree, so copy the whole source tree to a
        # separate directory before building.
        execute_process(
            COMMAND
                ${GIT_PROGRAM} clone --depth 1 "file://${TZ_SOURCE_DIR}" "${TZ_TMP_SOURCE_DIR}"
            COMMAND_ERROR_IS_FATAL ANY
        )

        if (APPLE)
            set(TZ_MAKEFLAGS "LDLIBS=${Intl_LIBRARY}")
        else()
            set(TZ_MAKEFLAGS)
        endif()

        execute_process(
            COMMAND
                ${GNU_MAKE} DESTDIR=${TZ_DIR} ${TZ_MAKEFLAGS} install
            WORKING_DIRECTORY 
                ${TZ_TMP_SOURCE_DIR}
            COMMAND_ERROR_IS_FATAL ANY
        )

        unset(TZ_MAKEFLAGS)

        # Step taken by Arch Linux packaging, but Nintendo apparently skips it
        # execute_process(
        #     COMMAND
        #         "${TZDB_LOCATION}/zic" -b fat -d ${TZDB_ZONEINFO} africa antarctica asia australasia europe northamerica southamerica etcetera backward factory
        #     WORKING_DIRECTORY
        #         "${TZDB_LOCATION}"
        #     COMMAND_ERROR_IS_FATAL ANY
        # )
    endif()

    execute_process(
        COMMAND
           ${CMAKE_COMMAND} -P ${PROJECT_SOURCE_DIR}/CMakeModules/list_directory.cmake false ON
        WORKING_DIRECTORY
            "${TZ_ZONEINFO_DIR}"
        OUTPUT_VARIABLE
            TZIF_SCAN
    )

    set(TZIF_LIST "")
    foreach(CANDIDATE ${TZIF_SCAN})
        if (CANDIDATE STREQUAL "\n")
            continue()
        endif()
        set(TZIF_FILE "${TZ_ZONEINFO_DIR}/${CANDIDATE}")
        file(READ "${TZIF_FILE}" HEADER LIMIT 4)
        string(SUBSTRING "${HEADER}" 0 4 HEADER) # Remove trailing newline
        if (HEADER STREQUAL "TZif")
            file(APPEND "${TZIF_LIST_FILE}" "${TZIF_FILE}\n")
        endif()
    endforeach()
endif()
