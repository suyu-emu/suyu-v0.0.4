# SPDX-FileCopyrightText: 2019 yuzu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

# Gets a UTC timestamp and sets the provided variable to it
function(get_timestamp _var)
    string(TIMESTAMP timestamp UTC)
    set(${_var} "${timestamp}" PARENT_SCOPE)
endfunction()

# generate git/build information
include(GetGitRevisionDescription)
if(NOT GIT_REF_SPEC)
    get_git_head_revision(GIT_REF_SPEC GIT_REV)
endif()
if(NOT GIT_DESC)
    git_describe(GIT_DESC --always --long --dirty)
endif()
if (NOT GIT_BRANCH)
  git_branch_name(GIT_BRANCH)
endif()
get_timestamp(BUILD_DATE)

git_get_exact_tag(GIT_TAG --tags)
if (GIT_TAG MATCHES "NOTFOUND")
  set(BUILD_VERSION "${GIT_DESC}")
  set(IS_DEV_BUILD true)
else()
  set(BUILD_VERSION ${GIT_TAG})
  set(IS_DEV_BUILD false)
endif()

# Generate cpp with Git revision from template
# Also if this is a CI build, add the build name (ie: Nightly, Canary) to the scm_rev file as well
set(REPO_NAME "Eden")
set(BUILD_ID ${GIT_BRANCH})
set(BUILD_FULLNAME "${REPO_NAME} ${BUILD_VERSION} ")

configure_file(scm_rev.cpp.in scm_rev.cpp @ONLY)
