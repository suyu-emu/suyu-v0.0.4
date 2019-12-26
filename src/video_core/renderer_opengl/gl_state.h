// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <type_traits>
#include <glad/glad.h>
#include "video_core/engines/maxwell_3d.h"

namespace OpenGL {

class OpenGLState {
public:
    OpenGLState();

    /// Get the currently active OpenGL state
    static OpenGLState GetCurState() {
        return cur_state;
    }

    /// Apply this state as the current OpenGL state
    void Apply();

private:
    static OpenGLState cur_state;
};
static_assert(std::is_trivially_copyable_v<OpenGLState>);

} // namespace OpenGL
