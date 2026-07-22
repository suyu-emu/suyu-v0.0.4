// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "shader_recompiler/frontend/ir/opcodes.h"

namespace Shader::IR {

namespace Detail {

OpcodeMeta META_TABLE[532] = {
#define OPCODE(name_token, type_token, ...)                                                        \
    {                                                                                              \
        .name{#name_token},                                                                        \
        .type = type_token,                                                                        \
        .arg_types{__VA_ARGS__},                                                                   \
    },
#include "opcodes.inc"
#undef OPCODE
};

u8 NUM_ARGS[532] = {
#define OPCODE(name_token, type_token, ...) u8(CalculateNumArgsOf(Opcode::name_token)),
#include "opcodes.inc"
#undef OPCODE
};

}

std::string_view NameOf(Opcode op) {
    return Detail::META_TABLE[static_cast<size_t>(op)].name;
}

} // namespace Shader::IR
