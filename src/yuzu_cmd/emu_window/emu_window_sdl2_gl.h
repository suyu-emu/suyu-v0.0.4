// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "core/frontend/emu_window.h"
#include "yuzu_cmd/emu_window/emu_window_sdl2.h"

class EmuWindow_SDL2_GL final : public EmuWindow_SDL2 {
public:
    explicit EmuWindow_SDL2_GL(bool fullscreen);
    ~EmuWindow_SDL2_GL();

    /// Swap buffers to display the next frame
    void SwapBuffers() override;

    /// Makes the graphics context current for the caller thread
    void MakeCurrent() override;

    /// Releases the GL context from the caller thread
    void DoneCurrent() override;

    std::unique_ptr<Core::Frontend::GraphicsContext> CreateSharedContext() const override;

private:
    /// Whether the GPU and driver supports the OpenGL extension required
    bool SupportsRequiredGLExtensions();

    using SDL_GLContext = void*;
    /// The OpenGL context associated with the window
    SDL_GLContext gl_context;
};
