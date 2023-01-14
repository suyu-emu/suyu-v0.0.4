// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/demangle.h"

namespace llvm {
char* itaniumDemangle(const char* mangled_name, char* buf, size_t* n, int* status);
}

namespace Common {

std::string DemangleSymbol(const std::string& mangled) {
    auto is_itanium = [](const std::string& name) -> bool {
        // A valid Itanium encoding requires 1-4 leading underscores, followed by 'Z'.
        auto pos = name.find_first_not_of('_');
        return pos > 0 && pos <= 4 && name[pos] == 'Z';
    };

    char* demangled = nullptr;
    if (is_itanium(mangled)) {
        demangled = llvm::itaniumDemangle(mangled.c_str(), nullptr, nullptr, nullptr);
    }

    if (!demangled) {
        return mangled;
    }

    std::string ret = demangled;
    std::free(demangled);
    return ret;
}

} // namespace Common
