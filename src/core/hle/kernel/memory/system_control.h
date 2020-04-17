// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace Kernel::Memory::SystemControl {

u64 GenerateRandomU64ForInit();

template <typename F>
u64 GenerateUniformRange(u64 min, u64 max, F f);

u64 GenerateRandomRange(u64 min, u64 max);

} // namespace Kernel::Memory::SystemControl
