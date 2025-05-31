set(BINARY_LIST_TXT ${CMAKE_ARGV3})
set(LIST_DIR_CMAKE ${CMAKE_ARGV4})

# Fill text file with zone names
# Issue: Hyphens/underscores are not handled the same way Nintendo handles them
function(get_files_nx TARG SUB_DIR)
    execute_process(
        COMMAND
            ${CMAKE_COMMAND} -P ${LIST_DIR_CMAKE} false OFF
        WORKING_DIRECTORY
            ${TARG}
        OUTPUT_VARIABLE
            FILE_LIST
    )
    list(SORT FILE_LIST)
    execute_process(
        COMMAND
            ${CMAKE_COMMAND} -P ${LIST_DIR_CMAKE} true OFF
        WORKING_DIRECTORY
            ${TARG}
        OUTPUT_VARIABLE
            DIR_LIST
    )

    foreach(FILE ${FILE_LIST})
        if(FILE STREQUAL "\n")
            continue()
        endif()
        list(REMOVE_ITEM DIR_LIST FILE)
        if (SUB_DIR)
            file(APPEND ${BINARY_LIST_TXT} "${SUB_DIR}/${FILE}\r\n")
        else()
            file(APPEND ${BINARY_LIST_TXT} "${FILE}\r\n")
        endif()
    endforeach()

    list(SORT DIR_LIST)

    foreach(DIR ${DIR_LIST})
        if (NOT DIR OR DIR STREQUAL "\n")
            continue()
        endif()
        if (SUB_DIR)
            get_files_nx(${TARG}/${DIR} ${SUB_DIR}/${DIR})
        else()
            get_files_nx(${TARG}/${DIR} ${DIR})
        endif()
    endforeach()
endfunction()

get_files_nx(${CMAKE_SOURCE_DIR} "")

