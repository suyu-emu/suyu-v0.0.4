set(SHADER_FILES
    block_linear_unswizzle_2d.comp
    block_linear_unswizzle_3d.comp
    convert_depth_to_float.frag
    convert_float_to_depth.frag
    full_screen_triangle.vert
    opengl_copy_bc4.comp
    opengl_present.frag
    opengl_present.vert
    pitch_unswizzle.comp
    vulkan_blit_color_float.frag
    vulkan_blit_depth_stencil.frag
    vulkan_present.frag
    vulkan_present.vert
    vulkan_quad_array.comp
    vulkan_quad_indexed.comp
    vulkan_uint8.comp
)

find_program(GLSLANGVALIDATOR "glslangValidator" REQUIRED)

set(GLSL_FLAGS "")

set(SHADER_INCLUDE ${CMAKE_CURRENT_BINARY_DIR}/include)
set(SHADER_DIR ${SHADER_INCLUDE}/video_core/host_shaders)
set(HOST_SHADERS_INCLUDE ${SHADER_INCLUDE} PARENT_SCOPE)

set(INPUT_FILE ${CMAKE_CURRENT_SOURCE_DIR}/source_shader.h.in)
set(HEADER_GENERATOR ${CMAKE_CURRENT_SOURCE_DIR}/StringShaderHeader.cmake)

foreach(FILENAME IN ITEMS ${SHADER_FILES})
    string(REPLACE "." "_" SHADER_NAME ${FILENAME})
    set(SOURCE_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${FILENAME})
    # Skip generating source headers on Vulkan exclusive files
    if (NOT ${FILENAME} MATCHES "vulkan.*")
        set(SOURCE_HEADER_FILE ${SHADER_DIR}/${SHADER_NAME}.h)
        add_custom_command(
            OUTPUT
                ${SOURCE_HEADER_FILE}
            COMMAND
                ${CMAKE_COMMAND} -P ${HEADER_GENERATOR} ${SOURCE_FILE} ${SOURCE_HEADER_FILE} ${INPUT_FILE}
            MAIN_DEPENDENCY
                ${SOURCE_FILE}
            DEPENDS
                ${INPUT_FILE}
                # HEADER_GENERATOR should be included here but msbuild seems to assume it's always modified
        )
        set(SHADER_HEADERS ${SHADER_HEADERS} ${SOURCE_HEADER_FILE})
    endif()
    # Skip compiling to SPIR-V OpenGL exclusive files
    if (NOT ${FILENAME} MATCHES "opengl.*")
        string(TOUPPER ${SHADER_NAME}_SPV SPIRV_VARIABLE_NAME)
        set(SPIRV_HEADER_FILE ${SHADER_DIR}/${SHADER_NAME}_spv.h)
        add_custom_command(
            OUTPUT
                ${SPIRV_HEADER_FILE}
            COMMAND
                ${GLSLANGVALIDATOR} -V --quiet ${GLSL_FLAGS} --variable-name ${SPIRV_VARIABLE_NAME} -o ${SPIRV_HEADER_FILE} ${SOURCE_FILE}
            MAIN_DEPENDENCY
                ${SOURCE_FILE}
        )
        set(SHADER_HEADERS ${SHADER_HEADERS} ${SPIRV_HEADER_FILE})
    endif()
endforeach()

add_custom_target(host_shaders
    DEPENDS
        ${SHADER_HEADERS}
    SOURCES
        ${SHADER_FILES}
)
