vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO yuzu-mirror/dynarmic
    REF ba8192d
    SHA512 8F840DD480556A265473CB467EB9AA30CE9D27F2B22CCFA79BCF39B3E3E3E40C4F1FCBEC78017E43EB289E5095608EE07C3DF087C0FD20BCC6B9A5D9626A561D
    HEAD_REF master
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DDYNARMIC_TESTS=OFF
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
