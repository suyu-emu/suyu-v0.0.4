# Disable tests in all externals supporting the standard option name
set(BUILD_TESTING OFF)

# Build only static externals
set(BUILD_SHARED_LIBS OFF)

# QuaZip
include(CPM)
set(CPM_SOURCE_CACHE ${CMAKE_SOURCE_DIR}/.cache/cpm)
set(CPM_USE_LOCAL_PACKAGES ON)

CPMAddPackage(
    NAME QuaZip-Qt6
    VERSION 1.3
    GIT_REPOSITORY "https://github.com/stachenov/quazip.git"
    GIT_TAG v1.5
    PATCHES
        ${CMAKE_SOURCE_DIR}/.patch/quazip/0001-strict.patch
        ${CMAKE_SOURCE_DIR}/.patch/quazip/0002-oldstyle.patch
        ${CMAKE_SOURCE_DIR}/.patch/quazip/0003-predecls.patch
        ${CMAKE_SOURCE_DIR}/.patch/quazip/0004-qt6-only.patch

    # thanks to 0004-qt6-only.patch, this isn't needed,
    # but we keep it since the patch is "technically" optional
    OPTIONS
        "QUAZIP_QT_MAJOR_VERSION 6"
        "QUAZIP_BZIP2 OFF"
)
