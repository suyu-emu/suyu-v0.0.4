// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include "core/frontend/emu_window.h"
#include "video_core/renderer_base.h"
#include "video_core/renderer_opengl/gl_rasterizer.h"

namespace VideoCore {

RendererBase::RendererBase(EmuWindow& window) : render_window{window} {}
RendererBase::~RendererBase() = default;

void RendererBase::UpdateCurrentFramebufferLayout() {
    const Layout::FramebufferLayout& layout = render_window.GetFramebufferLayout();

    render_window.UpdateCurrentFramebufferLayout(layout.width, layout.height);
}

void RendererBase::RefreshRasterizerSetting() {
    if (rasterizer == nullptr) {
        rasterizer = std::make_unique<RasterizerOpenGL>(render_window);
    }
}

} // namespace VideoCore
