// SPDX-FileCopyrightText: Copyright 2026 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/frontend/graphics_context.h"
#include "libretro_core/retro_emu_window.h"

namespace LibretroCore {

RetroEmuWindow::RetroEmuWindow() {
    window_info.type = Core::Frontend::WindowSystemType::Headless;
    window_info.render_surface = nullptr;
    UpdateCurrentFramebufferLayout(1280, 720);
}

RetroEmuWindow::~RetroEmuWindow() = default;

std::unique_ptr<Core::Frontend::GraphicsContext> RetroEmuWindow::CreateSharedContext() const {
    // The default GraphicsContext base is already a valid no-op shared
    // context (SwapBuffers/MakeCurrent/DoneCurrent all default to doing
    // nothing) - sufficient for headless operation.
    return std::make_unique<Core::Frontend::GraphicsContext>();
}

bool RetroEmuWindow::IsShown() const {
    // There is no real window to minimize; always report visible so the
    // renderer doesn't pause itself thinking it's hidden.
    return true;
}

} // namespace LibretroCore
