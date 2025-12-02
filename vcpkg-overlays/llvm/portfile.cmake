vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

# Suppress policy checks for issues that are difficult to resolve with LLVM's build system
set(VCPKG_POLICY_SKIP_ABSOLUTE_PATHS_CHECK enabled)
set(VCPKG_POLICY_SKIP_MISPLACED_CMAKE_FILES_CHECK enabled)
set(VCPKG_POLICY_SKIP_LIB_CMAKE_MERGE_CHECK enabled)
set(VCPKG_POLICY_ALLOW_EXES_IN_BIN enabled)
set(VCPKG_POLICY_DLLS_IN_STATIC_LIBRARY enabled)

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
        -DLLVM_ENABLE_ZLIB=ON
        -DLLVM_PARALLEL_LINK_JOBS=1
        # Disable shared library components that cause export generation issues
        -DLLVM_BUILD_LLVM_C_DYLIB=OFF
        -DLLVM_LINK_LLVM_DYLIB=OFF
        # Disable tools that cause llvm-nm crashes during static builds
        -DLLVM_BUILD_TOOLS=OFF
        -DLLVM_INCLUDE_TOOLS=OFF
        # Add memory safety options
        -DLLVM_ENABLE_CRASH_OVERRIDES=OFF
        -DLLVM_ENABLE_DUMP=OFF
    )
    
    # Limit parallel compilation to prevent resource exhaustion
    if(VCPKG_CONCURRENCY)
        math(EXPR SAFE_CONCURRENCY "${VCPKG_CONCURRENCY} / 2")
        if(SAFE_CONCURRENCY LESS 1)
            set(SAFE_CONCURRENCY 1)
        endif()
        list(APPEND LLVM_OPTIONS -DLLVM_PARALLEL_COMPILE_JOBS=${SAFE_CONCURRENCY})
    else()
        list(APPEND LLVM_OPTIONS -DLLVM_PARALLEL_COMPILE_JOBS=1)
    endif()
endif()

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}/llvm"
    OPTIONS ${LLVM_OPTIONS}
    MAYBE_UNUSED_VARIABLES
        LLVM_PARALLEL_COMPILE_JOBS
        LLVM_PARALLEL_LINK_JOBS
        LLVM_BUILD_LLVM_C_DYLIB
        LLVM_LINK_LLVM_DYLIB
        LLVM_BUILD_TOOLS
        LLVM_INCLUDE_TOOLS
        LLVM_ENABLE_CRASH_OVERRIDES
        LLVM_ENABLE_DUMP
)

vcpkg_cmake_build()

vcpkg_cmake_install()

# Fix CMake configuration files location and merge debug/release configs
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/llvm)
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

# Handle tools and bin directories for static builds
if(VCPKG_LIBRARY_LINKAGE STREQUAL "static")
    # Copy any essential tools before removing bin directories
    if(VCPKG_TARGET_IS_WINDOWS)
        if(EXISTS "${CURRENT_PACKAGES_DIR}/bin/llvm-tblgen.exe")
            vcpkg_copy_tools(TOOL_NAMES llvm-tblgen AUTO_CLEAN)
        endif()
    else()
        if(EXISTS "${CURRENT_PACKAGES_DIR}/bin/llvm-tblgen")
            vcpkg_copy_tools(TOOL_NAMES llvm-tblgen AUTO_CLEAN)
        endif()
    endif()
    
    # Remove bin directories as they shouldn't exist in static builds
    file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/bin" "${CURRENT_PACKAGES_DIR}/bin")
endif()

vcpkg_copy_pdbs()

file(INSTALL "${SOURCE_PATH}/LICENSE.TXT" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
