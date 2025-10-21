vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO baldurk/renderdoc
    REF v1.29.0
    SHA512 0
    HEAD_REF master
)

file(INSTALL "${SOURCE_PATH}/renderdoc/api" DESTINATION "${CURRENT_PACKAGES_DIR}/include/renderdoc")

file(INSTALL "${SOURCE_PATH}/LICENSE.md" DESTINATION "${CURRENT_PACKAGES_DIR}/share/renderdoc-api" RENAME copyright)

# Create a simple CMake config file
file(WRITE "${CURRENT_PACKAGES_DIR}/share/renderdoc-api/renderdoc-api-config.cmake" "")
