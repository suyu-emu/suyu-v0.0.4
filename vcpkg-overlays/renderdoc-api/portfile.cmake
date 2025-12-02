vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO baldurk/renderdoc
    REF v1.41
    SHA512 a3ba03f675dbd22f547310ac06f0830c933c122b6bf82912b079ccab609e0c5868c97ab27280a16cebd4f9eb083a9fc17470052c4e47f0e3c7d142ebaac28db4
)

# Verify that the API directory exists
if(NOT EXISTS "${SOURCE_PATH}/renderdoc/api")
    message(FATAL_ERROR "RenderDoc API directory not found at ${SOURCE_PATH}/renderdoc/api")
endif()

file(INSTALL "${SOURCE_PATH}/renderdoc/api" DESTINATION "${CURRENT_PACKAGES_DIR}/include/renderdoc")

file(INSTALL "${SOURCE_PATH}/LICENSE.md" DESTINATION "${CURRENT_PACKAGES_DIR}/share/renderdoc-api" RENAME copyright)

# Create a proper CMake config file
file(WRITE "${CURRENT_PACKAGES_DIR}/share/renderdoc-api/renderdoc-api-config.cmake" 
"# RenderDoc API CMake configuration file
# This file provides the RenderDoc API headers for graphics debugging
# The include directory path uses vcpkg variables for robustness.

if(NOT TARGET renderdoc-api::renderdoc-api)
    add_library(renderdoc-api::renderdoc-api INTERFACE IMPORTED)
    set_target_properties(renderdoc-api::renderdoc-api PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES \"\${VCPKG_INSTALLED_DIR}/\${VCPKG_TARGET_TRIPLET}/include/renderdoc\"
    )
endif()
")
