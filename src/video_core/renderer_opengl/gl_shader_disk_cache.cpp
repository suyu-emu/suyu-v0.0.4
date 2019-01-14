// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "video_core/renderer_opengl/gl_shader_disk_cache.h"

namespace OpenGL {

// Making sure sizes doesn't change by accident
static_assert(sizeof(BaseBindings) == 12);

} // namespace OpenGL