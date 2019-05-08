// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string_view>
#include <vector>
#include <glad/glad.h>
#include "common/common_types.h"
#include "common/math_util.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"

namespace OpenGL {

class CachedSurfaceView;

class BindBuffersRangePushBuffer {
public:
    BindBuffersRangePushBuffer(GLenum target);
    ~BindBuffersRangePushBuffer();

    void Setup(GLuint first_);

    void Push(GLuint buffer, GLintptr offset, GLsizeiptr size);

    void Bind() const;

private:
    GLenum target;
    GLuint first;
    std::vector<GLuint> buffers;
    std::vector<GLintptr> offsets;
    std::vector<GLsizeiptr> sizes;
};

void LabelGLObject(GLenum identifier, GLuint handle, VAddr addr, std::string_view extra_info = {});

} // namespace OpenGL
