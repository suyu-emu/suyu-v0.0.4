// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <string>
#include <string_view>
#include <llvm/Demangle/Demangle.h>

#include "common/demangle.h"
#include "common/scope_exit.h"

namespace Common {

std::string DemangleSymbol(const std::string& mangled) {
    if (mangled.size() > 0) {
        auto const is_itanium = [](std::string_view name) -> bool {
            // A valid Itanium encoding requires 1-4 leading underscores, followed by 'Z'.
            auto const pos = name.find_first_not_of('_');
            return pos > 0 && pos <= 4 && pos < name.size() && name[pos] == 'Z';
        };
        std::string ret = mangled;
        if (is_itanium(mangled)) {
            if (char* p = llvm::itaniumDemangle(mangled); p != nullptr) {
                ret = std::string{p};
                std::free(p);
            }
        }
        return ret;
    }
    return std::string{};
}

} // namespace Common
