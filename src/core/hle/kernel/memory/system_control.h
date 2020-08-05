// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace Kernel::Memory::SystemControl {

u64 GenerateRandomRange(u64 min, u64 max);

} // namespace Kernel::Memory::SystemControl
