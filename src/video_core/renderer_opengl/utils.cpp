// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string>
#include <fmt/format.h>
#include <glad/glad.h>

#include "common/assert.h"
#include "common/common_types.h"
#include "common/scope_exit.h"
#include "video_core/renderer_opengl/gl_state.h"
#include "video_core/renderer_opengl/gl_texture_cache.h"
#include "video_core/renderer_opengl/utils.h"
#include "video_core/surface.h"

namespace OpenGL {

using Tegra::Shader::TextureType;
using Tegra::Texture::SwizzleSource;

using VideoCore::Surface::SurfaceTarget;
using VideoCore::Surface::SurfaceType;

BindBuffersRangePushBuffer::BindBuffersRangePushBuffer(GLenum target) : target{target} {}

BindBuffersRangePushBuffer::~BindBuffersRangePushBuffer() = default;

void BindBuffersRangePushBuffer::Setup(GLuint first_) {
    first = first_;
    buffers.clear();
    offsets.clear();
    sizes.clear();
}

void BindBuffersRangePushBuffer::Push(GLuint buffer, GLintptr offset, GLsizeiptr size) {
    buffers.push_back(buffer);
    offsets.push_back(offset);
    sizes.push_back(size);
}

void BindBuffersRangePushBuffer::Bind() const {
    const std::size_t count{buffers.size()};
    DEBUG_ASSERT(count == offsets.size() && count == sizes.size());
    if (count == 0) {
        return;
    }
    glBindBuffersRange(target, first, static_cast<GLsizei>(count), buffers.data(), offsets.data(),
                       sizes.data());
}

SurfaceBlitter::SurfaceBlitter() {
    src_framebuffer.Create();
    dst_framebuffer.Create();
}

SurfaceBlitter::~SurfaceBlitter() = default;

void SurfaceBlitter::Blit(View src, View dst, const Common::Rectangle<u32>& src_rect,
                          const Common::Rectangle<u32>& dst_rect) const {
    const auto& src_params{src->GetSurfaceParams()};
    const auto& dst_params{dst->GetSurfaceParams()};

    OpenGLState prev_state{OpenGLState::GetCurState()};
    SCOPE_EXIT({ prev_state.Apply(); });

    OpenGLState state;
    state.draw.read_framebuffer = src_framebuffer.handle;
    state.draw.draw_framebuffer = dst_framebuffer.handle;
    state.ApplyFramebufferState();

    u32 buffers{};

    UNIMPLEMENTED_IF(src_params.target != SurfaceTarget::Texture2D);
    UNIMPLEMENTED_IF(dst_params.target != SurfaceTarget::Texture2D);

    const GLuint src_texture{src->GetTexture()};
    const GLuint dst_texture{dst->GetTexture()};

    if (src_params.type == SurfaceType::ColorTexture) {
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               src_texture, 0);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0,
                               0);

        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               dst_texture, 0);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0,
                               0);

        buffers = GL_COLOR_BUFFER_BIT;
    } else if (src_params.type == SurfaceType::Depth) {
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, src_texture,
                               0);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);

        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, dst_texture,
                               0);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);

        buffers = GL_DEPTH_BUFFER_BIT;
    } else if (src_params.type == SurfaceType::DepthStencil) {
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
                               src_texture, 0);

        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
                               dst_texture, 0);

        buffers = GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
    }

    glBlitFramebuffer(src_rect.left, src_rect.top, src_rect.right, src_rect.bottom, dst_rect.left,
                      dst_rect.top, dst_rect.right, dst_rect.bottom, buffers,
                      buffers == GL_COLOR_BUFFER_BIT ? GL_LINEAR : GL_NEAREST);
}

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
