// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace Kernel {

class KSystemControl {
public:
    KSystemControl() = default;

    static u64 GenerateRandomRange(u64 min, u64 max);
    static u64 GenerateRandomU64();
};

} // namespace Kernel
