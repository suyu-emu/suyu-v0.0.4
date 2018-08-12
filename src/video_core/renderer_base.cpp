// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include "core/frontend/emu_window.h"
#include "core/settings.h"
#include "video_core/renderer_base.h"
#include "video_core/renderer_opengl/gl_rasterizer.h"

namespace VideoCore {

RendererBase::RendererBase(Core::Frontend::EmuWindow& window) : render_window{window} {
    RefreshBaseSettings();
}

RendererBase::~RendererBase() = default;

void RendererBase::RefreshBaseSettings() {
    RefreshRasterizerSetting();
    UpdateCurrentFramebufferLayout();

    renderer_settings.use_framelimiter = Settings::values.toggle_framelimit;
}

void RendererBase::RefreshRasterizerSetting() {
    if (rasterizer == nullptr) {
        rasterizer = std::make_unique<RasterizerOpenGL>(render_window);
    }
}

void RendererBase::UpdateCurrentFramebufferLayout() {
    const Layout::FramebufferLayout& layout = render_window.GetFramebufferLayout();

    render_window.UpdateCurrentFramebufferLayout(layout.width, layout.height);
}

} // namespace VideoCore
