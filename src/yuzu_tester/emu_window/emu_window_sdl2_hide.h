// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/frontend/emu_window.h"

struct SDL_Window;

namespace InputCommon {
class InputSubsystem;
}

class EmuWindow_SDL2_Hide : public Core::Frontend::EmuWindow {
public:
    explicit EmuWindow_SDL2_Hide();
    ~EmuWindow_SDL2_Hide();

    /// Whether the screen is being shown or not.
    bool IsShown() const override;

    std::unique_ptr<Core::Frontend::GraphicsContext> CreateSharedContext() const override;

private:
    /// Whether the GPU and driver supports the OpenGL extension required
    bool SupportsRequiredGLExtensions();

    std::unique_ptr<InputCommon::InputSubsystem> input_subsystem;

    /// Internal SDL2 render window
    SDL_Window* render_window;

    using SDL_GLContext = void*;
    /// The OpenGL context associated with the window
    SDL_GLContext gl_context;
};
