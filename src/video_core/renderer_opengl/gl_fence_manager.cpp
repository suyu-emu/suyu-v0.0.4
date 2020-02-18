// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"

#include "video_core/renderer_opengl/gl_fence_manager.h"

namespace OpenGL {

GLInnerFence::GLInnerFence(GPUVAddr address, u32 payload)
    : VideoCommon::FenceBase(address, payload), sync_object{} {}

GLInnerFence::~GLInnerFence() = default;

void GLInnerFence::Queue() {
    ASSERT(sync_object.handle == 0);
    sync_object.Create();
}

bool GLInnerFence::IsSignaled() const {
    ASSERT(sync_object.handle != 0);
    GLsizei length;
    GLint sync_status;
    glGetSynciv(sync_object.handle, GL_SYNC_STATUS, sizeof(GLint), &length, &sync_status);
    return sync_status == GL_SIGNALED;
}

void GLInnerFence::Wait() {
    ASSERT(sync_object.handle != 0);
    while (glClientWaitSync(sync_object.handle, 0, 1000) == GL_TIMEOUT_EXPIRED)
        ;
}

FenceManagerOpenGL::FenceManagerOpenGL(Core::System& system, VideoCore::RasterizerInterface& rasterizer,
                               TextureCacheOpenGL& texture_cache, OGLBufferCache& buffer_cache)
    : GenericFenceManager(system, rasterizer, texture_cache, buffer_cache) {}

Fence FenceManagerOpenGL::CreateFence(GPUVAddr addr, u32 value) {
    return std::make_shared<GLInnerFence>(addr, value);
}

void FenceManagerOpenGL::QueueFence(Fence& fence) {
    fence->Queue();
}

bool FenceManagerOpenGL::IsFenceSignaled(Fence& fence) {
    return fence->IsSignaled();
}

void FenceManagerOpenGL::WaitFence(Fence& fence) {
    fence->Wait();
}

} // namespace OpenGL
