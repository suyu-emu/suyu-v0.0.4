// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include "shader_recompiler/backend/bindings.h"
#include "shader_recompiler/frontend/ir/program.h"
#include "shader_recompiler/profile.h"
#include "shader_recompiler/runtime_info.h"

namespace Shader::Backend::MSL {

[[nodiscard]] std::string EmitMSL(const Profile& profile, const RuntimeInfo& runtime_info,
                                  IR::Program& program, Bindings& bindings);

[[nodiscard]] inline std::string EmitMSL(const Profile& profile, IR::Program& program) {
    Bindings binding;
    return EmitMSL(profile, {}, program, binding);
}

} // namespace Shader::Backend::MSL
