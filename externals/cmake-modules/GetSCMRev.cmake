# SPDX-FileCopyrightText: Copyright 2025 crueter
# SPDX-License-Identifier: GPL-3.0-or-later

## GetSCMRev ##
# Name is self explanatory. Gets revision information from files, OR from git.
# Prioritizes GIT-TAG, GIT-REFSPEC, GIT-COMMIT, GIT-RELEASE files within the root directory,
# otherwise grabs stuff from Git.

# loosely based on Ryan Pavlik's work
find_package(Git QUIET)

# commit: git rev-parse HEAD
# tag:    git describe --tags --abbrev=0
# branch: git rev-parse --abbrev-ref=HEAD

function(run_git_command variable)
    if(NOT GIT_FOUND)
        set(${variable} "GIT-NOTFOUND" PARENT_SCOPE)
        return()
    endif()

    execute_process(COMMAND
        "${GIT_EXECUTABLE}"
            ${ARGN}
        WORKING_DIRECTORY
            "${CMAKE_SOURCE_DIR}"
        RESULT_VARIABLE
            res
        OUTPUT_VARIABLE
            out
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE)

    if(NOT res EQUAL 0)
        set(out "${out}-${res}-NOTFOUND")
    endif()

    set(${variable} "${out}" PARENT_SCOPE)
endfunction()

function(trim var)
    string(REGEX REPLACE "\n" "" new "${${var}}")
    set(${var} ${new} PARENT_SCOPE)
endfunction()

set(TAG_FILE ${CMAKE_SOURCE_DIR}/GIT-TAG)
set(REF_FILE ${CMAKE_SOURCE_DIR}/GIT-REFSPEC)
set(COMMIT_FILE ${CMAKE_SOURCE_DIR}/GIT-COMMIT)
set(RELEASE_FILE ${CMAKE_SOURCE_DIR}/GIT-RELEASE)

if (EXISTS ${REF_FILE} AND EXISTS ${COMMIT_FILE})
    file(READ ${REF_FILE} GIT_REFSPEC)
    file(READ ${COMMIT_FILE} GIT_COMMIT)
else()
    run_git_command(GIT_COMMIT  rev-parse HEAD)
    run_git_command(GIT_REFSPEC rev-parse --abbrev-ref HEAD)

    if (GIT_REFSPEC MATCHES "NOTFOUND")
        set(GIT_REFSPEC 1.0.0)
        set(GIT_COMMIT stable)
    endif()
endif()

if (EXISTS ${TAG_FILE})
    file(READ ${TAG_FILE} GIT_TAG)
else()
    run_git_command(GIT_TAG describe --tags --abbrev=0)
    if (GIT_TAG MATCHES "NOTFOUND")
        set(GIT_TAG "${GIT_REFSPEC}")
    endif()
endif()

if (EXISTS ${RELEASE_FILE})
    file(READ ${RELEASE_FILE} GIT_RELEASE)
    trim(GIT_RELEASE)
    message(STATUS "[GetSCMRev] Git release: ${GIT_RELEASE}")
endif()

trim(GIT_REFSPEC)
trim(GIT_COMMIT)
trim(GIT_TAG)

message(STATUS "[GetSCMRev] Git commit: ${GIT_COMMIT}")
message(STATUS "[GetSCMRev] Git tag: ${GIT_TAG}")
message(STATUS "[GetSCMRev] Git refspec: ${GIT_REFSPEC}")
