/* This file is part of the dynarmic project.
 * Copyright (c) 2023 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#include "dynarmic/ir/basic_block.h"
#include "dynarmic/ir/microinstruction.h"

namespace Dynarmic::Optimization {

void NamingPass(IR::Block& block) {
    unsigned name = 1;
    for (auto& inst : block) {
        inst.SetName(name++);
    }
}

}  // namespace Dynarmic::Optimization
