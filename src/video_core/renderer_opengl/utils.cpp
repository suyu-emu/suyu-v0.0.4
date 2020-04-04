// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string>
#include <vector>

#include <fmt/format.h>
#include <glad/glad.h>

#include "common/common_types.h"
#include "video_core/renderer_opengl/gl_state_tracker.h"
#include "video_core/renderer_opengl/utils.h"

namespace OpenGL {

void LabelGLObject(GLenum identifier, GLuint handle, VAddr addr, std::string_view extra_info) {
    if (!GLAD_GL_KHR_debug) {
        // We don't need to throw an error as this is just for debugging
        return;
    }

    std::string object_label;
    if (extra_info.empty()) {
        switch (identifier) {
        case GL_TEXTURE:
            object_label = fmt::format("Texture@0x{:016X}", addr);
            break;
        case GL_PROGRAM:
            object_label = fmt::format("Shader@0x{:016X}", addr);
            break;
        default:
            object_label = fmt::format("Object(0x{:X})@0x{:016X}", identifier, addr);
            break;
        }
    } else {
        object_label = fmt::format("{}@0x{:016X}", extra_info, addr);
    }
    glObjectLabel(identifier, handle, -1, static_cast<const GLchar*>(object_label.c_str()));
}

} // namespace OpenGL
