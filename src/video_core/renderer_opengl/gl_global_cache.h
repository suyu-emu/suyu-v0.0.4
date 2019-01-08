// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <glad/glad.h>

#include "common/common_types.h"
#include "video_core/rasterizer_cache.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"

namespace OpenGL {

namespace GLShader {
class GlobalMemoryEntry;
} // namespace GLShader

class RasterizerOpenGL;
class CachedGlobalRegion;
using GlobalRegion = std::shared_ptr<CachedGlobalRegion>;

class CachedGlobalRegion final : public RasterizerCacheObject {
public:
    explicit CachedGlobalRegion(VAddr addr, u32 size);

    /// Gets the address of the shader in guest memory, required for cache management
    VAddr GetAddr() const {
        return addr;
    }

    /// Gets the size of the shader in guest memory, required for cache management
    std::size_t GetSizeInBytes() const {
        return size;
    }

    /// Gets the GL program handle for the buffer
    GLuint GetBufferHandle() const {
        return buffer.handle;
    }

    // TODO(Rodrigo): When global memory is written (STG), implement flushing
    void Flush() override {
        UNIMPLEMENTED();
    }

private:
    VAddr addr{};
    u32 size{};

    OGLBuffer buffer;
};

class GlobalRegionCacheOpenGL final : public RasterizerCache<GlobalRegion> {
public:
    explicit GlobalRegionCacheOpenGL(RasterizerOpenGL& rasterizer);
};

} // namespace OpenGL
