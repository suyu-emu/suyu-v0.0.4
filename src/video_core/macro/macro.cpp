// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "common/logging/log.h"
#include "core/settings.h"
#include "video_core/macro/macro.h"
#include "video_core/macro/macro_interpreter.h"
#include "video_core/macro/macro_jit_x64.h"

namespace Tegra {

void MacroEngine::AddCode(u32 method, u32 data) {
    uploaded_macro_code[method].push_back(data);
}

void MacroEngine::Execute(u32 method, const std::vector<u32>& parameters) {
    auto compiled_macro = macro_cache.find(method);
    if (compiled_macro != macro_cache.end()) {
        compiled_macro->second->Execute(parameters, method);
    } else {
        // Macro not compiled, check if it's uploaded and if so, compile it
        auto macro_code = uploaded_macro_code.find(method);
        if (macro_code == uploaded_macro_code.end()) {
            UNREACHABLE_MSG("Macro 0x{0:x} was not uploaded", method);
            return;
        }
        macro_cache[method] = Compile(macro_code->second);
        macro_cache[method]->Execute(parameters, method);
    }
}

std::unique_ptr<MacroEngine> GetMacroEngine(Engines::Maxwell3D& maxwell3d) {
    if (Settings::values.disable_macro_jit) {
        return std::make_unique<MacroInterpreter>(maxwell3d);
    }
#ifdef ARCHITECTURE_x86_64
    return std::make_unique<MacroJITx64>(maxwell3d);
#else
    return std::make_unique<MacroInterpreter>(maxwell3d);
#endif
}

} // namespace Tegra
