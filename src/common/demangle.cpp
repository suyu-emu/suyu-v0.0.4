// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <string>
#include <string_view>
#ifdef _WIN32
#include <llvm/Demangle/Demangle.h>
#else
#include <cxxabi.h>
#endif
#include "common/demangle.h"

static bool IsItanium(std::string_view name) {
    // A valid Itanium encoding requires 1-4 leading underscores, followed by 'Z'.
    auto const pos = name.find_first_not_of('_');
    return pos > 0 && pos <= 4 && pos < name.size() && name[pos] == 'Z';
}

namespace Common {
std::string DemangleSymbol(const std::string& mangled) {
    if (mangled.size() > 0) {
        if (IsItanium(mangled)) {
#ifdef _WIN32
            // requires the use of llvm
            if (char* p = llvm::itaniumDemangle(mangled); p != nullptr) {
                std::string ret = std::string{p};
                std::free(p);
                return ret;
            }
#else
            // can safely use libc++ and glibcxx provided demangling functions
            // it's available since 2008(!) so no system should have issues with it
            // see https://gcc.gnu.org/onlinedocs/libstdc++/libstdc++-html-USERS-4.3/a01696.html
            int status;
            if (char* p = abi::__cxa_demangle(mangled.c_str(), NULL, NULL, &status); p != nullptr) {
                std::string ret = std::string{p};
                std::free(p);
                return ret;
            }
#endif
        }
        return mangled;
    }
    return std::string{};
}
} // namespace Common
