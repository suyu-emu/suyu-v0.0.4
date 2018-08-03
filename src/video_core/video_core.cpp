// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include "common/logging/log.h"
#include "video_core/renderer_base.h"
#include "video_core/renderer_opengl/renderer_opengl.h"
#include "video_core/video_core.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Video Core namespace

namespace VideoCore {

std::unique_ptr<RendererBase> g_renderer; ///< Renderer plugin

std::atomic<bool> g_toggle_framelimit_enabled;

/// Initialize the video core
bool Init(EmuWindow& emu_window) {
    g_renderer = std::make_unique<RendererOpenGL>(emu_window);
    if (g_renderer->Init()) {
        LOG_DEBUG(Render, "initialized OK");
    } else {
        LOG_CRITICAL(Render, "initialization failed !");
        return false;
    }
    return true;
}

/// Shutdown the video core
void Shutdown() {
    g_renderer.reset();

    LOG_DEBUG(Render, "shutdown OK");
}

} // namespace VideoCore
