vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO baldurk/renderdoc
    REF v1.27.0
    SHA512 0
    HEAD_REF master
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

if(NOT TARGET renderdoc-api::renderdoc-api)
    add_library(renderdoc-api::renderdoc-api INTERFACE IMPORTED)
    set_target_properties(renderdoc-api::renderdoc-api PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES \"\${CMAKE_CURRENT_LIST_DIR}/../../include\"
    )
endif()
")
