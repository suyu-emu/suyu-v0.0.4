vcpkg_from_git(
    OUT_SOURCE_PATH SOURCE_PATH
    URL https://git.eden-emu.dev/eden-emu/dynarmic.git
    REF master
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DDYNARMIC_TESTS=OFF
        -DDYNARMIC_USE_LLVM=OFF
)

vcpkg_cmake_build()

vcpkg_cmake_install()

vcpkg_copy_pdbs()

# Fix CMake targets if they exist
if(EXISTS "${CURRENT_PACKAGES_DIR}/lib/cmake/dynarmic")
    vcpkg_fixup_cmake_targets(CONFIG_PATH lib/cmake/dynarmic)
elseif(EXISTS "${CURRENT_PACKAGES_DIR}/share/dynarmic")
    vcpkg_fixup_cmake_targets()
endif()

file(INSTALL "${SOURCE_PATH}/LICENSE.txt" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
