include(CPMUtil)

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

if ("riscv" IN_LIST ARCHITECTURE)
    add_subdirectory(biscuit)

    AddPackage(
        NAME biscuit
        VERSION 0.9.1
        REPO "lioncash/biscuit"
        SHA 76b0be8dae
        HASH 47d55ed02d032d6cf3dc107c6c0a9aea686d5f25aefb81d1af91db027b6815bd5add1755505e19d76625feeb17aa2db6cd1668fe0dad2e6a411519bde6ca4489
    )
endif()

# catch

# TODO(crueter): dedup
if (NOT TARGET Catch2::Catch2WithMain)
    if (DYNARMIC_TESTS)
        find_package(Catch2 3.0.1 REQUIRED)
    endif()
endif()

# fmt

if (NOT TARGET fmt::fmt)
    # fmtlib formatting library
    set(FMT_INSTALL ON)
    add_subdirectory(fmt)
endif()

# mcl
AddPackage(
    NAME mcl
    VERSION 0.1.12
    REPO "azahar-emu/mcl"
    SHA 7b08d83418
    HASH f943bac39c1879986decad7a442ff4288eaeca4a2907684c7914e115a55ecc43c2782ded85c0835763fe04e40d5c82220ce864423e489e648e408a84f54dc4f3
    OPTIONS
        "MCL_INSTALL OFF"
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

AddPackage(
    NAME unordered_dense
    REPO "Lizzie841/unordered_dense"
    SHA e59d30b7b1
    HASH 71eff7bd9ba4b9226967bacd56a8ff000946f8813167cb5664bb01e96fb79e4e220684d824fe9c59c4d1cc98c606f13aff05b7940a1ed8ab3c95d6974ee34fa0
    FIND_PACKAGE_ARGUMENTS "CONFIG"
    OPTIONS
        "UNORDERED_DENSE_INSTALL OFF"
)

# xbyak
# uncomment if in an independent repo

# if (NOT TARGET xbyak::xbyak)
#     if ("x86_64" IN_LIST ARCHITECTURE)
#         add_subdirectory(xbyak)
#     endif()
# endif()

# zydis

if ("x86_64" IN_LIST ARCHITECTURE)
    AddPackage(
        NAME Zycore
        REPO "zyantific/zycore-c"
        SHA 75a36c45ae
        HASH 15aa399f39713e042c4345bc3175c82f14dca849fde2a21d4f591f62c43e227b70d868d8bb86beb5f4eb68b1d6bd3792cdd638acf89009e787e3d10ee7401924
    )

    AddPackage(
        NAME Zydis
        VERSION 4
        REPO "zyantific/zydis"
        SHA c2d2bab025
        HASH 7b48f213ff7aab2926f8c9c65195959143bebbfb2b9a25051ffd8b8b0f1baf1670d9739781de674577d955925f91ac89376e16b476a03828c84e2fd765d45020
        OPTIONS
            "ZYDIS_BUILD_TOOLS OFF"
            "ZYDIS_BUILD_EXAMPLES OFF"
            "ZYDIS_BUILD_DOXYGEN OFF"
            "ZYAN_ZYCORE_PATH ${Zycore_SOURCE_DIR}"
            "CMAKE_DISABLE_FIND_PACKAGE_Doxygen ON"
    )
endif()
