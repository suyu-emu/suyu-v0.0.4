// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <glad/glad.h>

#include "video_core/renderer_opengl/gl_global_cache.h"
#include "video_core/renderer_opengl/gl_rasterizer.h"
#include "video_core/renderer_opengl/utils.h"

namespace OpenGL {

CachedGlobalRegion::CachedGlobalRegion(VAddr addr, u32 size) : addr{addr}, size{size} {
    buffer.Create();
    // Bind and unbind the buffer so it gets allocated by the driver
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer.handle);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    LabelGLObject(GL_BUFFER, buffer.handle, addr, "GlobalMemory");
}

GlobalRegionCacheOpenGL::GlobalRegionCacheOpenGL(RasterizerOpenGL& rasterizer)
    : RasterizerCache{rasterizer} {}

} // namespace OpenGL
