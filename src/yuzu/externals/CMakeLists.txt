# Disable tests in all externals supporting the standard option name
set(BUILD_TESTING OFF)

# Build only static externals
set(BUILD_SHARED_LIBS OFF)

# QuaZip
include(CPM)
set(CPM_USE_LOCAL_PACKAGES ON)

CPMAddPackage(
    NAME QuaZip-Qt6
    VERSION 1.3
    GIT_REPOSITORY "https://github.com/crueter/quazip-qt6.git"
    GIT_TAG v1.5-qt6
)
