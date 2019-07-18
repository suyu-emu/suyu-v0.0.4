// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/alignment.h"

namespace Kernel {

using PhysicalMemory = std::vector<u8, Common::AlignmentAllocator<u8, 256>>;

}
