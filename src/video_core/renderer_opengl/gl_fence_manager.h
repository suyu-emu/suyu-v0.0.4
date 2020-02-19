// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <glad/glad.h>

#include "common/common_types.h"
#include "video_core/fence_manager.h"
#include "video_core/renderer_opengl/gl_buffer_cache.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/gl_texture_cache.h"

namespace OpenGL {

class GLInnerFence : public VideoCommon::FenceBase {
public:
    GLInnerFence(u32 payload, bool is_stubbed);
    GLInnerFence(GPUVAddr address, u32 payload, bool is_stubbed);
    ~GLInnerFence();

    void Queue();

    bool IsSignaled() const;

    void Wait();

private:
    OGLSync sync_object;
};

using Fence = std::shared_ptr<GLInnerFence>;
using GenericFenceManager = VideoCommon::FenceManager<Fence, TextureCacheOpenGL, OGLBufferCache>;

class FenceManagerOpenGL final : public GenericFenceManager {
public:
    FenceManagerOpenGL(Core::System& system, VideoCore::RasterizerInterface& rasterizer,
                       TextureCacheOpenGL& texture_cache, OGLBufferCache& buffer_cache);

protected:
    Fence CreateFence(u32 value, bool is_stubbed) override;
    Fence CreateFence(GPUVAddr addr, u32 value, bool is_stubbed) override;
    void QueueFence(Fence& fence) override;
    bool IsFenceSignaled(Fence& fence) override;
    void WaitFence(Fence& fence) override;
};

} // namespace OpenGL
