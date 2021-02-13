// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace Kernel {

enum class KMemoryRegionType : u32 {};

enum class KMemoryRegionAttr : typename std::underlying_type<KMemoryRegionType>::type {
    CarveoutProtected = 0x04000000,
    DidKernelMap = 0x08000000,
    ShouldKernelMap = 0x10000000,
    UserReadOnly = 0x20000000,
    NoUserMap = 0x40000000,
    LinearMapped = 0x80000000,
};

} // namespace Kernel
