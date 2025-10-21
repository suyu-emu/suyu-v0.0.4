vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO llvm/llvm-project
    REF llvmorg-17.0.6
    SHA512 5300a452e706c1b6183ba300233804d97e4468d2588c2c2e0cf59e56ee5c83f20b7e03f5c0782198c34c63653b3e12d7407e4e8bb8214bae7e6532fa22730443
    HEAD_REF main
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}/llvm"
    OPTIONS
        -DLLVM_INCLUDE_TESTS=OFF
        -DLLVM_INCLUDE_EXAMPLES=OFF
        -DLLVM_INCLUDE_BENCHMARKS=OFF
        -DLLVM_TARGETS_TO_BUILD=X86
        -DBUILD_SHARED_LIBS=OFF
)

vcpkg_cmake_build()

vcpkg_cmake_install()

vcpkg_copy_pdbs()

file(INSTALL "${SOURCE_PATH}/LICENSE.TXT" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
