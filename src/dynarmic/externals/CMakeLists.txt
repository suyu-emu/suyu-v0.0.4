# Explicitly include CPMUtil here since we have a separate cpmfile for dynarmic
set(CPMUTIL_JSON_FILE ${CMAKE_CURRENT_SOURCE_DIR}/cpmfile.json)
include(CPMUtil)

# Always build externals as static libraries, even when dynarmic is built as shared
set(BUILD_SHARED_LIBS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set_property(DIRECTORY PROPERTY EXCLUDE_FROM_ALL ON)

# Allow options shadowing with normal variables when subproject use old cmake policy
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

# Disable tests in all externals supporting the standard option name
set(BUILD_TESTING OFF)

# For libraries that already come with a CMakeLists file,
# simply add the directory to that file as a subdirectory
# to have CMake automatically recognize them.

# biscuit

if ("riscv" IN_LIST ARCHITECTURE)
    add_subdirectory(biscuit)

    AddJsonPackage(
        NAME biscuit
        BUNDLED_PACKAGE ${DYNARMIC_USE_BUNDLED_EXTERNALS}
    )
endif()

# catch

# if (NOT TARGET Catch2::Catch2WithMain)
#     if (DYNARMIC_TESTS)
#         find_package(Catch2 3.0.1 REQUIRED)
#     endif()
# endif()

# fmt

# if (NOT TARGET fmt::fmt)
#     # fmtlib formatting library
#     set(FMT_INSTALL ON)
#     add_subdirectory(fmt)
# endif()

# mcl
AddJsonPackage(
    NAME mcl
    BUNDLED_PACKAGE ${DYNARMIC_USE_BUNDLED_EXTERNALS}
)

# oaknut

# if (NOT TARGET merry::oaknut)
#     if ("arm64" IN_LIST ARCHITECTURE)
#         add_subdirectory(oaknut)
#     elseif (DYNARMIC_TESTS)
#         add_subdirectory(oaknut EXCLUDE_FROM_ALL)
#     endif()
# endif()

# unordered_dense

AddJsonPackage(
    NAME unordered-dense
    BUNDLED_PACKAGE ${DYNARMIC_USE_BUNDLED_EXTERNALS}
)

# xbyak
# uncomment if in an independent repo

# if (NOT TARGET xbyak::xbyak)
#     if ("x86_64" IN_LIST ARCHITECTURE)
#         add_subdirectory(xbyak)
#     endif()
# endif()

# zydis

# TODO(crueter): maybe it's just Gentoo but zydis system package really sucks
if ("x86_64" IN_LIST ARCHITECTURE)
    set(CMAKE_DISABLE_FIND_PACKAGE_Doxygen ON)
    # TODO(crueter): system zycore doesn't work with zydis
    AddJsonPackage(zycore)

    AddJsonPackage(
        NAME zydis
        BUNDLED_PACKAGE ${DYNARMIC_USE_BUNDLED_EXTERNALS}
    )
endif()
