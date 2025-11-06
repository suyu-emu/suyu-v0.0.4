// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include "core/frontend/emu_window.h"
#include "suyu_cmd/emu_window/emu_window_sdl2.h"

namespace Core {
class System;
}

namespace InputCommon {
class InputSubsystem;
}

class EmuWindow_SDL2_MTL final : public EmuWindow_SDL2 {
public:
    explicit EmuWindow_SDL2_MTL(InputCommon::InputSubsystem* input_subsystem_, Core::System& system,
                                bool fullscreen);
    ~EmuWindow_SDL2_MTL() override;

    std::unique_ptr<Core::Frontend::GraphicsContext> CreateSharedContext() const override;
};
