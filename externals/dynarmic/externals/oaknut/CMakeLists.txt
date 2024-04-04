cmake_minimum_required(VERSION 3.8)
project(oaknut LANGUAGES CXX VERSION 2.0.2)

# Determine if we're built as a subproject (using add_subdirectory)
# or if this is the master project.
set(MASTER_PROJECT OFF)
if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(MASTER_PROJECT ON)
endif()

# Disable in-source builds
set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
if ("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
    message(SEND_ERROR "In-source builds are not allowed.")
endif()

# Source project files
set(header_files
    ${CMAKE_CURRENT_SOURCE_DIR}/include/oaknut/code_block.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/oaknut/dual_code_block.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/oaknut/feature_detection/cpu_feature.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/oaknut/feature_detection/feature_detection.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/oaknut/feature_detection/id_registers.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/oaknut/impl/arm64_encode_helpers.inc.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/oaknut/impl/cpu_feature.inc.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/oaknut/impl/enum.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/oaknut/impl/imm.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/oaknut/impl/list.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/oaknut/impl/mnemonics_fpsimd_v8.0.inc.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/oaknut/impl/mnemonics_fpsimd_v8.1.inc.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/oaknut/impl/mnemonics_fpsimd_v8.2.inc.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/oaknut/impl/mnemonics_generic_v8.0.inc.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/oaknut/impl/mnemonics_generic_v8.1.inc.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/oaknut/impl/mnemonics_generic_v8.2.inc.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/oaknut/impl/multi_typed_name.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/oaknut/impl/offset.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/oaknut/impl/overloaded.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/oaknut/impl/reg.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/oaknut/impl/string_literal.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/oaknut/oaknut.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/oaknut/oaknut_exception.hpp
)

include(GNUInstallDirs)

# Library definition
add_library(oaknut INTERFACE)
add_library(merry::oaknut ALIAS oaknut)
target_sources(oaknut INTERFACE "$<BUILD_INTERFACE:${header_files}>")
target_include_directories(oaknut INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_features(oaknut INTERFACE cxx_std_20)

# Tests
if (MASTER_PROJECT)
    include(CTest)
endif()

if (BUILD_TESTING)
    option(OAKNUT_USE_BUNDLED_CATCH "Use the embedded Catch2 submodule" OFF)
    if (OAKNUT_USE_BUNDLED_CATCH)
        add_subdirectory(externals/catch)
    else()
        find_package(Catch2 3 REQUIRED)
    endif()

    add_executable(oaknut-tests
        tests/_feature_detect.cpp
        tests/basic.cpp
        tests/fpsimd.cpp
        tests/general.cpp
        tests/rand_int.hpp
        tests/vector_code_gen.cpp
    )
    target_include_directories(oaknut-tests PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    target_link_libraries(oaknut-tests PRIVATE Catch2::Catch2WithMain merry::oaknut)
    if (MSVC)
        target_compile_options(oaknut-tests PRIVATE
            /experimental:external
            /external:W0
            /external:anglebrackets
            /W4
            /WX
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
            /Zi
            /Zo
            /EHsc
            /Zc:externConstexpr # Allows external linkage for variables declared "extern constexpr", as the standard permits.
            /Zc:inline          # Omits inline functions from object-file output.
            /Zc:throwingNew     # Assumes new (without std::nothrow) never returns null.
            /volatile:iso       # Use strict standard-abiding volatile semantics
        )
    else()
        target_compile_options(oaknut-tests PRIVATE -Wall -Wextra -Wcast-qual -pedantic -pedantic-errors -Wfatal-errors -Wno-missing-braces)
    endif()

    add_test(oaknut-tests oaknut-tests --durations yes)
endif()

# Install
include(CMakePackageConfigHelpers)

install(TARGETS oaknut EXPORT oaknutTargets)
install(EXPORT oaknutTargets
    NAMESPACE merry::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/oaknut"
)

configure_package_config_file("${CMAKE_CURRENT_SOURCE_DIR}/oaknutConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/oaknutConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/oaknut"
)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/oaknutConfigVersion.cmake"
    COMPATIBILITY SameMajorVersion
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/oaknutConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/oaknutConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/oaknut"
)
install(DIRECTORY
    "${CMAKE_CURRENT_SOURCE_DIR}/include/oaknut"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
