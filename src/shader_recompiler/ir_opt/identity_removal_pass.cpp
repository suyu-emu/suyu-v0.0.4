// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <vector>
#include <boost/container/small_vector.hpp>

#include "shader_recompiler/frontend/ir/basic_block.h"
#include "shader_recompiler/frontend/ir/value.h"
#include "shader_recompiler/ir_opt/passes.h"

namespace Shader::Optimization {

void IdentityRemovalPass(IR::Program& program) {
    boost::container::small_vector<IR::Inst*, 16> to_invalidate;
    for (IR::Block* const block : program.blocks) {
        for (auto it = block->begin(); it != block->end();) {
            const size_t num_args{it->NumArgs()};
            for (size_t i = 0; i < num_args; ++i) {
                IR::Value arg = it->Arg(i);
                if (arg.IsIdentity()) {
                    do { // Pointer chasing (3-derefs)
                        arg = arg.Inst()->Arg(0);
                    } while (arg.IsIdentity());
                    it->SetArg(i, arg);
                }
            }

            if (it->GetOpcode() == IR::Opcode::Identity || it->GetOpcode() == IR::Opcode::Void) {
                to_invalidate.push_back(&*it);
                it = block->Instructions().erase(it);
            } else {
                ++it;
            }
        }
    }
    for (IR::Inst* const inst : to_invalidate)
        inst->Invalidate();
}

} // namespace Shader::Optimization
