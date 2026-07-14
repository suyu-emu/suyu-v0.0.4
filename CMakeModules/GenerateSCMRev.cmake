# SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# SPDX-FileCopyrightText: 2019 yuzu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

# generate git/build information
include(GetSCMRev)

function(get_timestamp _var)
    string(TIMESTAMP timestamp UTC)
    set(${_var} "${timestamp}" PARENT_SCOPE)
endfunction()

get_timestamp(BUILD_DATE)

if (DEFINED GIT_RELEASE)
    set(BUILD_VERSION "${GIT_TAG}")
    set(GIT_REFSPEC "${GIT_RELEASE}")
    set(IS_DEV_BUILD false)
else()
    string(SUBSTRING ${GIT_COMMIT} 0 10 BUILD_VERSION)
    set(BUILD_VERSION "${BUILD_VERSION}-${GIT_REFSPEC}")
    set(IS_DEV_BUILD true)
endif()

if (NIGHTLY_BUILD)
    set(IS_NIGHTLY_BUILD true)
else()
    set(IS_NIGHTLY_BUILD false)
endif()

set(GIT_DESC ${BUILD_VERSION})

# Generate cpp with Git revision from template

# TODO(crueter): Stable releases feed.
set(BUILD_AUTO_UPDATE_STABLE_REPO "")
set(BUILD_AUTO_UPDATE_STABLE_API "")
set(BUILD_AUTO_UPDATE_STABLE_API_PATH "")

set(BUILD_AUTO_UPDATE_API_PATH "")
if (NIGHTLY_BUILD)
    set(BUILD_AUTO_UPDATE_WEBSITE "")
    set(BUILD_AUTO_UPDATE_API "")
    set(BUILD_AUTO_UPDATE_REPO "")
    set(REPO_NAME "suyu Nightly")
else()
    set(BUILD_AUTO_UPDATE_WEBSITE "")
    set(BUILD_AUTO_UPDATE_API "")
    set(BUILD_AUTO_UPDATE_REPO "")
    set(REPO_NAME "suyu")
endif()

set(BUILD_ID ${GIT_REFSPEC})
set(BUILD_FULLNAME "${REPO_NAME} ${BUILD_VERSION} ")
set(CXX_COMPILER "${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")

configure_file(scm_rev.cpp.in scm_rev.cpp @ONLY)
