// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <glad/glad.h>

#include "video_core/renderer_opengl/gl_query_cache.h"

namespace OpenGL {

HostCounter::HostCounter(GLenum target) {
    query.Create(target);
}

HostCounter::~HostCounter() = default;

void HostCounter::UpdateState(bool enabled) {
    if (enabled) {
        Enable();
    } else {
        Disable();
    }
}

void HostCounter::Reset() {
    counter = 0;
    Disable();
}

u64 HostCounter::Query() {
    if (!is_beginned) {
        return counter;
    }
    Disable();
    u64 value;
    glGetQueryObjectui64v(query.handle, GL_QUERY_RESULT, &value);
    Enable();

    counter += value;
    return counter;
}

void HostCounter::Enable() {
    if (is_beginned) {
        return;
    }
    is_beginned = true;
    glBeginQuery(GL_SAMPLES_PASSED, query.handle);
}

void HostCounter::Disable() {
    if (!is_beginned) {
        return;
    }
    glEndQuery(GL_SAMPLES_PASSED);
    is_beginned = false;
}

} // namespace OpenGL
