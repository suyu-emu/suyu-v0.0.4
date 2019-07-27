// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <glad/glad.h>

#include "common/common_types.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"

namespace OpenGL {

class HostCounter final {
public:
    explicit HostCounter(GLenum target);
    ~HostCounter();

    /// Enables or disables the counter as required.
    void UpdateState(bool enabled);

    /// Resets the counter disabling it if needed.
    void Reset();

    /// Returns the current value of the query.
    /// @note It may harm precision of future queries if the counter is not disabled.
    u64 Query();

private:
    /// Enables the counter when disabled.
    void Enable();

    /// Disables the counter when enabled.
    void Disable();

    OGLQuery query;     ///< OpenGL query.
    u64 counter{};      ///< Added values of the counter.
    bool is_beginned{}; ///< True when the OpenGL query is beginned.
};

} // namespace OpenGL
