set(SHADER_FILES
    opengl_present.frag
    opengl_present.vert
)

set(SHADER_INCLUDE ${CMAKE_CURRENT_BINARY_DIR}/include)
set(HOST_SHADERS_INCLUDE ${SHADER_INCLUDE} PARENT_SCOPE)

set(SHADER_DIR ${SHADER_INCLUDE}/video_core/host_shaders)
add_custom_command(
    OUTPUT
        ${SHADER_DIR}
    COMMAND
        ${CMAKE_COMMAND} -E make_directory ${SHADER_DIR}
)

set(INPUT_FILE ${CMAKE_CURRENT_SOURCE_DIR}/source_shader.h.in)
set(HEADER_GENERATOR ${CMAKE_CURRENT_SOURCE_DIR}/StringShaderHeader.cmake)

foreach(FILENAME IN ITEMS ${SHADER_FILES})
    string(REPLACE "." "_" SHADER_NAME ${FILENAME})
    set(SOURCE_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${FILENAME})
    set(HEADER_FILE ${SHADER_DIR}/${SHADER_NAME}.h)
    add_custom_command(
        OUTPUT
            ${HEADER_FILE}
        COMMAND
            ${CMAKE_COMMAND} -P ${HEADER_GENERATOR} ${SOURCE_FILE} ${HEADER_FILE} ${INPUT_FILE}
        MAIN_DEPENDENCY
            ${SOURCE_FILE}
        DEPENDS
            ${HEADER_GENERATOR}
            ${INPUT_FILE}
    )
    set(SHADER_HEADERS ${SHADER_HEADERS} ${HEADER_FILE})
endforeach()

add_custom_target(host_shaders
    DEPENDS
        ${SHADER_HEADERS}
    SOURCES
        ${SHADER_FILES}
)
