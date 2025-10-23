vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO llvm/llvm-project
    REF llvmorg-17.0.6
    SHA512 5300a452e706c1b6183ba300233804d97e4468d2588c2c2e0cf59e56ee5c83f20b7e03f5c0782198c34c63653b3e12d7407e4e8bb8214bae7e6532fa22730443
    HEAD_REF main
)

# Configure build options with Windows-specific settings
set(LLVM_OPTIONS
    -DLLVM_INCLUDE_TESTS=OFF
    -DLLVM_INCLUDE_EXAMPLES=OFF
    -DLLVM_INCLUDE_BENCHMARKS=OFF
    -DLLVM_TARGETS_TO_BUILD=X86
    -DBUILD_SHARED_LIBS=OFF
    -DLLVM_ENABLE_RTTI=ON
    -DLLVM_ENABLE_EH=ON
)

# Add Windows-specific build configurations
if(VCPKG_TARGET_IS_WINDOWS)
    list(APPEND LLVM_OPTIONS
        -DLLVM_USE_CRT_DEBUG=MTd
        -DLLVM_USE_CRT_RELEASE=MT
        -DLLVM_ENABLE_ZLIB=ON
        -DLLVM_PARALLEL_LINK_JOBS=1
    )
    
    # Limit parallel compilation to prevent resource exhaustion
    if(VCPKG_CONCURRENCY)
        list(APPEND LLVM_OPTIONS -DLLVM_PARALLEL_COMPILE_JOBS=${VCPKG_CONCURRENCY})
    else()
        list(APPEND LLVM_OPTIONS -DLLVM_PARALLEL_COMPILE_JOBS=2)
    endif()
endif()

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}/llvm"
    OPTIONS ${LLVM_OPTIONS}
    MAYBE_UNUSED_VARIABLES
        LLVM_USE_CRT_DEBUG
        LLVM_USE_CRT_RELEASE
        LLVM_PARALLEL_COMPILE_JOBS
        LLVM_PARALLEL_LINK_JOBS
)

vcpkg_cmake_build()

vcpkg_cmake_install()

vcpkg_copy_pdbs()

file(INSTALL "${SOURCE_PATH}/LICENSE.TXT" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
