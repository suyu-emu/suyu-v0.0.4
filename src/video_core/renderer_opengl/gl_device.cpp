// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstddef>
#include <glad/glad.h>

#include "video_core/renderer_opengl/gl_device.h"

namespace OpenGL {

namespace {
template <typename T>
T GetInteger(GLenum pname) {
    GLint temporary;
    glGetIntegerv(pname, &temporary);
    return static_cast<T>(temporary);
}
} // Anonymous namespace

Device::Device() = default;

void Device::Initialize() {
    uniform_buffer_alignment = GetInteger<std::size_t>(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT);
}

} // namespace OpenGL
