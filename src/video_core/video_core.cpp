// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include "video_core/renderer_base.h"
#include "video_core/renderer_opengl/renderer_opengl.h"
#include "video_core/video_core.h"

namespace VideoCore {

std::unique_ptr<RendererBase> CreateRenderer(Core::Frontend::EmuWindow& emu_window) {
    return std::make_unique<RendererOpenGL>(emu_window);
}

} // namespace VideoCore
