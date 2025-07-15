# Disable tests in all externals supporting the standard option name
set(BUILD_TESTING OFF)

# Build only static externals
set(BUILD_SHARED_LIBS OFF)

# QuaZip
set(QUAZIP_QT_MAJOR_VERSION 6)
set(QUAZIP_BZIP2 OFF)

include(CPM)
set(CPM_SOURCE_CACHE ${CMAKE_SOURCE_DIR}/.cache/cpm)
set(CPM_USE_LOCAL_PACKAGES ON)

CPMAddPackage(
    NAME QuaZip-Qt6
    VERSION 1.3
    GIT_REPOSITORY "https://github.com/stachenov/quazip.git"
    GIT_TAG v1.5
    PATCHES
        ${CMAKE_SOURCE_DIR}/.ci/patch/0001-quazip-strict.patch
        ${CMAKE_SOURCE_DIR}/.ci/patch/0002-quazip-fetchcontent.patch
)

if (NOT MSVC AND NOT "QuaZip-Qt6" IN_LIST CPM_PACKAGES)
    message(STATUS "QUAZIP DIR: ${CPM_PACKAGES}")
    target_compile_options(QuaZip PRIVATE
        -Wno-error=shadow
        -Wno-error=missing-declarations
    )
endif()
