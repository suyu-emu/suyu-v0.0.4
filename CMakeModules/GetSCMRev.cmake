# SPDX-FileCopyrightText: 2025 crueter
# SPDX-License-Identifier: GPL-3.0-or-later

include(GetGitRevisionDescription)

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
  get_git_head_revision(GIT_REFSPEC GIT_COMMIT)
  git_branch_name(GIT_REFSPEC)
  if (GIT_REFSPEC MATCHES "NOTFOUND")
    set(GIT_REFSPEC 1.0.0)
    set(GIT_COMMIT stable)
  endif()
endif()

if (EXISTS ${TAG_FILE})
  file(READ ${TAG_FILE} GIT_TAG)
else()
  git_describe(GIT_TAG --tags --abbrev=0)
  if (GIT_TAG MATCHES "NOTFOUND")
    set(GIT_TAG "${GIT_REFSPEC}")
  endif()
endif()

if (EXISTS ${RELEASE_FILE})
  file(READ ${RELEASE_FILE} GIT_RELEASE)
  trim(GIT_RELEASE)
  message(STATUS "Git release: ${GIT_RELEASE}")
endif()

trim(GIT_REFSPEC)
trim(GIT_COMMIT)
trim(GIT_TAG)

message(STATUS "Git commit: ${GIT_COMMIT}")
message(STATUS "Git tag: ${GIT_TAG}")
message(STATUS "Git refspec: ${GIT_REFSPEC}")
