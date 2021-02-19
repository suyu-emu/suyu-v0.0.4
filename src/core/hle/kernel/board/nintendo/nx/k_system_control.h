// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace Kernel::Board::Nintendo::Nx {

class KSystemControl {
public:
    class Init {
    public:
        static bool ShouldIncreaseThreadResourceLimit();
    };

    static u64 GenerateRandomRange(u64 min, u64 max);
    static u64 GenerateRandomU64();
};

} // namespace Kernel::Board::Nintendo::Nx
