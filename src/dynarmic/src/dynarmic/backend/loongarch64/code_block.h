// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>

#include "common/common_types.h"

namespace Dynarmic::Backend::LoongArch64 {

class CodeBlock {
public:
    template<typename T>
    T ptr() const noexcept {
        return reinterpret_cast<T>(mem);
    }

private:
    u8* mem = nullptr;
};

}  // namespace Dynarmic::Backend::LoongArch64
