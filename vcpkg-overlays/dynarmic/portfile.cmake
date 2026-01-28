vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO yuzu-mirror/dynarmic
    REF ba8192d
    SHA512 8F840DD480556A265473CB467EB9AA30CE9D27F2B22CCFA79BCF39B3E3E3E40C4F1FCBEC78017E43EB289E5095608EE07C3DF087C0FD20BCC6B9A5D9626A561D
    HEAD_REF master
)

# Remove bundled Xbyak to avoid conflicts with the vcpkg xbyak port (the upstream tree
# may vend-in Xbyak headers which conflict with the xbyak package installed by
# vcpkg). Remove common vendor paths before configuring so the install step does
# not attempt to write duplicate files into the install tree.
file(REMOVE_RECURSE
    "${SOURCE_PATH}/third_party/xbyak"
    "${SOURCE_PATH}/xbyak"
    "${SOURCE_PATH}/extern/xbyak"
    "${SOURCE_PATH}/externals/xbyak"
)

# Configure with conditional options so Ninja does not get generator expressions in the command line
set(DYNARMIC_CONFIGURE_OPTIONS
    -DDYNARMIC_TESTS=OFF
    -DDYNARMIC_USE_LLVM=OFF
)

# Force static build on Windows by prepending a BUILD_SHARED_LIBS cache override to upstream CMakeLists.txt
if(VCPKG_TARGET_IS_WINDOWS)
    file(READ "${SOURCE_PATH}/CMakeLists.txt" DYNARMIC_CMAKETXT)
    string(FIND "${DYNARMIC_CMAKETXT}" "set(BUILD_SHARED_LIBS" _found)
    if(_found EQUAL -1)
        file(WRITE "${SOURCE_PATH}/CMakeLists.txt" "set(BUILD_SHARED_LIBS OFF CACHE BOOL \"\" FORCE)\n${DYNARMIC_CMAKETXT}")
    endif()
    list(APPEND DYNARMIC_CONFIGURE_OPTIONS -DBUILD_SHARED_LIBS=OFF)
endif()

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        ${DYNARMIC_CONFIGURE_OPTIONS}
)

vcpkg_cmake_build()

vcpkg_cmake_install()

# Upstream may install headers in debug; remove duplicates per vcpkg policy.
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

vcpkg_copy_pdbs()

# Fix CMake targets if they exist
if(EXISTS "${CURRENT_PACKAGES_DIR}/lib/cmake/dynarmic")
    vcpkg_fixup_cmake_targets(CONFIG_PATH lib/cmake/dynarmic)
elseif(EXISTS "${CURRENT_PACKAGES_DIR}/share/dynarmic")
    vcpkg_fixup_cmake_targets()
endif()

file(INSTALL "${SOURCE_PATH}/LICENSE.txt" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
