// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string_view>
#include <vector>
#include <glad/glad.h>
#include "common/common_types.h"

namespace OpenGL {

class VertexArrayPushBuffer final {
public:
    explicit VertexArrayPushBuffer();
    ~VertexArrayPushBuffer();

    void Setup(GLuint vao_);

    void SetIndexBuffer(const GLuint* buffer);

    void SetVertexBuffer(GLuint binding_index, const GLuint* buffer, GLintptr offset,
                         GLsizei stride);

    void Bind();

private:
    struct Entry {
        GLuint binding_index{};
        const GLuint* buffer{};
        GLintptr offset{};
        GLsizei stride{};
    };

    GLuint vao{};
    const GLuint* index_buffer{};
    std::vector<Entry> vertex_buffers;
};

class BindBuffersRangePushBuffer final {
public:
    explicit BindBuffersRangePushBuffer(GLenum target);
    ~BindBuffersRangePushBuffer();

    void Setup();

    void Push(GLuint binding, const GLuint* buffer, GLintptr offset, GLsizeiptr size);

    void Bind();

private:
    struct Entry {
        GLuint binding;
        const GLuint* buffer;
        GLintptr offset;
        GLsizeiptr size;
    };

    GLenum target;
    std::vector<Entry> entries;
};

void LabelGLObject(GLenum identifier, GLuint handle, VAddr addr, std::string_view extra_info = {});

} // namespace OpenGL
