# Always build externals as static libraries, even when dynarmic is built as shared
if (BUILD_SHARED_LIBS)
    set(BUILD_SHARED_LIBS OFF)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    set_property(DIRECTORY PROPERTY EXCLUDE_FROM_ALL ON)
endif()

# Allow options shadowing with normal variables when subproject use old cmake policy
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

# Disable tests in all externals supporting the standard option name
set(BUILD_TESTING OFF)

# For libraries that already come with a CMakeLists file,
# simply add the directory to that file as a subdirectory
# to have CMake automatically recognize them.

# biscuit

if (NOT TARGET biscuit::biscuit)
    if ("riscv" IN_LIST ARCHITECTURE)
        add_subdirectory(biscuit)
    endif()
endif()

# catch

if (NOT TARGET Catch2::Catch2WithMain)
    if (DYNARMIC_TESTS)
        add_subdirectory(catch EXCLUDE_FROM_ALL)
    endif()
endif()

# fmt

if (NOT TARGET fmt::fmt)
    # fmtlib formatting library
    set(FMT_INSTALL ON)
    add_subdirectory(fmt)
endif()

# mcl

if (NOT TARGET merry::mcl)
    set(MCL_INSTALL ON)
    add_subdirectory(mcl)
endif()

# oaknut

if (NOT TARGET merry::oaknut)
    if ("arm64" IN_LIST ARCHITECTURE)
        add_subdirectory(oaknut)
    elseif (DYNARMIC_TESTS)
        add_subdirectory(oaknut EXCLUDE_FROM_ALL)
    endif()
endif()

# unordered_dense

if (NOT TARGET ankerl::unordered_dense)
    set(UNORDERED_DENSE_INSTALL ON)
    add_subdirectory(unordered_dense)
endif()

# xbyak
# uncomment if in an independent repo.

# if (NOT TARGET xbyak::xbyak)
#     if ("x86_64" IN_LIST ARCHITECTURE)
#         add_subdirectory(xbyak)
#     endif()
# endif()

# zydis

if (NOT TARGET Zydis::Zydis)
    if ("x86_64" IN_LIST ARCHITECTURE)
        set(ZYDIS_BUILD_TOOLS OFF)
        set(ZYDIS_BUILD_EXAMPLES OFF)
        set(ZYDIS_BUILD_DOXYGEN OFF)
        set(ZYAN_ZYCORE_PATH "${CMAKE_CURRENT_LIST_DIR}/zycore-c" CACHE PATH "")
        set(CMAKE_DISABLE_FIND_PACKAGE_Doxygen ON)
        if (NOT TARGET Zydis)
            add_subdirectory(zydis)
        endif()
    endif()
endif()
