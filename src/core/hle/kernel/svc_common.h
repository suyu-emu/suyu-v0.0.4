// Copyright 2020 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace Kernel::Svc {

constexpr s32 ArgumentHandleCountMax = 0x40;
constexpr u32 HandleWaitMask{1u << 30};

} // namespace Kernel::Svc
