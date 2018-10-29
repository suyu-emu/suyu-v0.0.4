// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <glad/glad.h>
#include "common/common_types.h"

namespace OpenGL {

void LabelGLObject(GLenum identifier, GLuint handle, VAddr addr, std::string extra_info = "");

} // namespace OpenGL