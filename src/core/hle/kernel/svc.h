// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace Core {
class System;
}

namespace Kernel {

void CallSVC(Core::System& system, u32 immediate);

} // namespace Kernel
