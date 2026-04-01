# SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

set(FIDELITYFX_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/externals/FidelityFX-FSR/ffx-fsr)

set(FIDELITYFX_FILES
    ${FIDELITYFX_INCLUDE_DIR}/ffx_a.h
    ${FIDELITYFX_INCLUDE_DIR}/ffx_fsr1.h
)

set(GLSL_INCLUDES
    fidelityfx_fsr.frag
    ${FIDELITYFX_FILES}
)

set(SHADER_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/astc_decoder.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/blit_color_float.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/block_linear_unswizzle_2d.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/block_linear_unswizzle_3d.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/block_linear_unswizzle_3d_bcn.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/convert_abgr8_to_d24s8.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/convert_abgr8_to_d32f.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/convert_d32f_to_abgr8.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/convert_d24s8_to_abgr8.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/convert_depth_to_float.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/convert_float_to_depth.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/convert_msaa_to_non_msaa.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/convert_non_msaa_to_msaa.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/convert_s8d24_to_abgr8.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/full_screen_triangle.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/fxaa.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/fxaa.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/opengl_convert_s8d24.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/opengl_copy_bc4.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/opengl_fidelityfx_fsr.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/opengl_fidelityfx_fsr_easu.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/opengl_fidelityfx_fsr_rcas.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/opengl_lmem_warmup.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/opengl_present.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/opengl_present.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/opengl_present_scaleforce.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/opengl_smaa.glsl
    ${CMAKE_CURRENT_SOURCE_DIR}/pitch_unswizzle.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/present_area.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/present_bicubic.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/present_zero_tangent.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/present_bspline.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/present_mitchell.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/present_gaussian.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/present_lanczos.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/present_spline1.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/present_mmpx.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/queries_prefix_scan_sum.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/queries_prefix_scan_sum_nosubgroups.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/resolve_conditional_render.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/smaa_edge_detection.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/smaa_edge_detection.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/smaa_blending_weight_calculation.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/smaa_blending_weight_calculation.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/smaa_neighborhood_blending.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/smaa_neighborhood_blending.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan_blit_depth_stencil.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan_color_clear.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan_color_clear.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan_depthstencil_clear.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan_fidelityfx_fsr.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan_fidelityfx_fsr_easu_fp16.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan_fidelityfx_fsr_easu_fp32.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan_fidelityfx_fsr_rcas_fp16.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan_fidelityfx_fsr_rcas_fp32.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan_present.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan_present.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan_present_scaleforce_fp16.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan_present_scaleforce_fp32.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan_quad_indexed.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan_turbo_mode.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan_uint8.comp
)

if (PLATFORM_HAIKU)
    # glslangValidator WILL crash, glslang will not
    set(GLSLANGVALIDATOR "glslang")
else()
    # Normal sane platform who doesn't have a CRASHING glslangValidator
    find_program(GLSLANGVALIDATOR "glslangValidator")
endif()

if ("${GLSLANGVALIDATOR}" STREQUAL "GLSLANGVALIDATOR-NOTFOUND")
    message(FATAL_ERROR "Required program `glslangValidator` not found.")
endif()

set(GLSL_FLAGS "")
set(SPIR_V_VERSION "spirv1.3")
set(QUIET_FLAG "--quiet")

set(SHADER_INCLUDE ${CMAKE_CURRENT_BINARY_DIR}/include)
set(SHADER_DIR ${SHADER_INCLUDE}/video_core/host_shaders)
set(HOST_SHADERS_INCLUDE ${SHADER_INCLUDE} PARENT_SCOPE)

set(INPUT_FILE ${CMAKE_CURRENT_SOURCE_DIR}/source_shader.h.in)
set(HEADER_GENERATOR ${CMAKE_CURRENT_SOURCE_DIR}/StringShaderHeader.cmake)

# Check if `--quiet` is available on host's glslangValidator version
# glslangValidator prints to STDERR iff an unrecognized flag is passed to it
execute_process(
    COMMAND
        ${GLSLANGVALIDATOR} ${QUIET_FLAG}
    ERROR_VARIABLE
        GLSLANG_ERROR
    # STDOUT variable defined to silence unnecessary output during CMake configuration
    OUTPUT_VARIABLE
        GLSLANG_OUTPUT
)

if (NOT GLSLANG_ERROR STREQUAL "")
    message(WARNING "Refusing to use unavailable flag `${QUIET_FLAG}` on `${GLSLANGVALIDATOR}`")
    set(QUIET_FLAG "")
endif()

foreach(SOURCE_FILE IN ITEMS ${SHADER_FILES})
    get_filename_component(FILENAME ${SOURCE_FILE} NAME)
    string(REPLACE "." "_" SHADER_NAME ${FILENAME})
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
                ${GLSLANGVALIDATOR} -V ${QUIET_FLAG} -I"${FIDELITYFX_INCLUDE_DIR}" ${GLSL_FLAGS} --variable-name ${SPIRV_VARIABLE_NAME} -o ${SPIRV_HEADER_FILE} ${SOURCE_FILE} --target-env ${SPIR_V_VERSION}
            MAIN_DEPENDENCY
                ${SOURCE_FILE}
        )
        set(SHADER_HEADERS ${SHADER_HEADERS} ${SPIRV_HEADER_FILE})
    endif()
endforeach()

foreach(FILEPATH IN ITEMS ${FIDELITYFX_FILES})
    get_filename_component(FILENAME ${FILEPATH} NAME)
    string(REPLACE "." "_" HEADER_NAME ${FILENAME})
    set(SOURCE_FILE ${FILEPATH})
    set(SOURCE_HEADER_FILE ${SHADER_DIR}/${HEADER_NAME}.h)
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
endforeach()

set(SHADER_SOURCES ${SHADER_FILES})
list(APPEND SHADER_SOURCES ${GLSL_INCLUDES})

add_custom_target(host_shaders
    DEPENDS
        ${SHADER_HEADERS}
    SOURCES
        ${SHADER_SOURCES}
)
