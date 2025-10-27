# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.12)
project(dynarmic LANGUAGES C CXX ASM VERSION 6.7.0)

# Determine if we're built as a subproject (using add_subdirectory)
# or if this is the master project.
set(MASTER_PROJECT OFF)
if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  set(MASTER_PROJECT ON)
endif()

if (MASTER_PROJECT)
    include(CTest)
endif()

# Dynarmic project options
option(DYNARMIC_ENABLE_CPU_FEATURE_DETECTION "Turning this off causes dynarmic to assume the host CPU doesn't support anything later than SSE3" ON)

option(DYNARMIC_ENABLE_NO_EXECUTE_SUPPORT "Enables support for systems that require W^X" ${PLATFORM_OPENBSD})

option(DYNARMIC_IGNORE_ASSERTS "Ignore asserts" OFF)
option(DYNARMIC_TESTS_USE_UNICORN "Enable fuzzing tests against unicorn" OFF)
CMAKE_DEPENDENT_OPTION(DYNARMIC_USE_LLVM "Support disassembly of jitted x86_64 code using LLVM" OFF "NOT YUZU_DISABLE_LLVM" OFF)

option(DYNARMIC_USE_PRECOMPILED_HEADERS "Use precompiled headers" OFF)

option(DYNARMIC_INSTALL "Install dynarmic headers and CMake files" OFF)
option(DYNARMIC_USE_BUNDLED_EXTERNALS "Use all bundled externals (useful when e.g. cross-compiling)" OFF)
option(DYNARMIC_ENABLE_LTO "Enable LTO" OFF)

# Default to a Release build
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
    message(STATUS "Defaulting to a Release build")
endif()

# Set hard requirements for C++
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Disable in-source builds
# set(CMAKE_DISABLE_SOURCE_CHANGES ON)
# set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
if ("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
    message(SEND_ERROR "In-source builds are not allowed.")
endif()

# Add the module directory to the list of paths
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/CMakeModules")

# Arch detection
include(DetectArchitecture)
if (NOT DEFINED ARCHITECTURE)
    message(FATAL_ERROR "Unsupported architecture encountered. Ending CMake generation.")
endif()
message(STATUS "Target architecture: ${ARCHITECTURE}")

# Compiler flags
if (MSVC)
    set(DYNARMIC_CXX_FLAGS
        /experimental:external
        /external:W0
        /external:anglebrackets
        /W4
        /w44263 # Non-virtual member function hides base class virtual function
        /w44265 # Class has virtual functions, but destructor is not virtual
        /w44456 # Declaration of 'var' hides previous local declaration
        /w44457 # Declaration of 'var' hides function parameter
        /w44458 # Declaration of 'var' hides class member
        /w44459 # Declaration of 'var' hides global definition
        /w44946 # Reinterpret-cast between related types
        /wd4592 # Symbol will be dynamically initialized (implementation limitation)
        /permissive- # Stricter C++ standards conformance
        /MP
        /Zo
        /EHsc
        /Zc:externConstexpr # Allows external linkage for variables declared "extern constexpr", as the standard permits.
        /Zc:inline          # Omits inline functions from object-file output.
        /Zc:throwingNew     # Assumes new (without std::nothrow) never returns null.
        /volatile:iso       # Use strict standard-abiding volatile semantics
        /bigobj             # Increase number of sections in .obj files
        /DNOMINMAX)

    if (WIN32 AND (CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo"))
        string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
        string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
   endif()

    if (CXX_CLANG)
        list(APPEND DYNARMIC_CXX_FLAGS
             -Qunused-arguments
             -Wno-missing-braces)
    endif()
else()
    set(DYNARMIC_CXX_FLAGS
        -Wall
        -Wextra
        -Wcast-qual
        -pedantic
        -Wno-missing-braces)

    if (CXX_GCC)
        # GCC produces bogus -Warray-bounds warnings from xbyak headers for code paths that are not
        # actually reachable.  Specifically, it happens in cases where some code casts an Operand&
        # to Address& after first checking isMEM(), and that code is inlined in a situation where
        # GCC knows that the variable is actually a Reg64.  isMEM() will never return true for a
        # Reg64, but GCC doesn't know that.
        list(APPEND DYNARMIC_CXX_FLAGS -Wno-array-bounds)
        list(APPEND DYNARMIC_CXX_FLAGS -Wstack-usage=4096)
    endif()

    if (CXX_CLANG)
        # Bracket depth determines maximum size of a fold expression in Clang since 9c9974c3ccb6.
        # And this in turns limits the size of a std::array.
        list(APPEND DYNARMIC_CXX_FLAGS -fbracket-depth=1024)
        # Clang mistakenly blames CMake for using unused arguments during compilation
        list(APPEND DYNARMIC_CXX_FLAGS -Wno-unused-command-line-argument)
    endif()
endif()

find_package(Boost 1.57 REQUIRED)
find_package(fmt 9 CONFIG)

# Pull in externals CMakeLists for libs where available
add_subdirectory(externals)

find_package(mcl 0.1.12 REQUIRED)

if ("arm64" IN_LIST ARCHITECTURE OR DYNARMIC_TESTS)
    find_package(oaknut 2.0.1 CONFIG)
endif()

if ("riscv" IN_LIST ARCHITECTURE)
    find_package(biscuit 0.9.1 REQUIRED)
endif()

if ("x86_64" IN_LIST ARCHITECTURE)
    find_package(xbyak 7 CONFIG)
    find_package(zycore REQUIRED)
    find_package(zydis 4 REQUIRED)
endif()

if (DYNARMIC_USE_LLVM)
    find_package(LLVM REQUIRED)
    separate_arguments(LLVM_DEFINITIONS)
endif()

if (DYNARMIC_TESTS)
    find_package(Catch2 3 CONFIG)
    if (DYNARMIC_TESTS_USE_UNICORN)
        find_package(Unicorn REQUIRED)
    endif()
endif()

# Dynarmic project files
add_subdirectory(src/dynarmic)
if (DYNARMIC_TESTS)
    add_subdirectory(tests)
endif()

#
# Install
#
if (DYNARMIC_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    install(TARGETS dynarmic EXPORT dynarmicTargets)
    install(EXPORT dynarmicTargets
        NAMESPACE dynarmic::
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/dynarmic"
    )

    configure_package_config_file(CMakeModules/dynarmicConfig.cmake.in
        dynarmicConfig.cmake
        INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/dynarmic"
    )
    write_basic_package_version_file(dynarmicConfigVersion.cmake
        COMPATIBILITY SameMajorVersion
    )
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/dynarmicConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/dynarmicConfigVersion.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/dynarmic"
    )

    install(DIRECTORY src/dynarmic TYPE INCLUDE FILES_MATCHING PATTERN "*.h")
endif()
