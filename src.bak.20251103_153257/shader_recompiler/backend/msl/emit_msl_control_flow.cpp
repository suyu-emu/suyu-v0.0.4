// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "shader_recompiler/backend/msl/emit_msl_instructions.h"
#include "shader_recompiler/backend/msl/msl_emit_context.h"
#include "shader_recompiler/exception.h"

namespace Shader::Backend::MSL {

void EmitJoin(EmitContext&) {
    throw NotImplementedException("Join shouldn't be emitted");
}

void EmitDemoteToHelperInvocation(EmitContext& ctx) {
    ctx.Add("discard_fragment();");
}

} // namespace Shader::Backend::MSL
