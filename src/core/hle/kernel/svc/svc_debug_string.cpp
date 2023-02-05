// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/core.h"
#include "core/hle/kernel/svc.h"
#include "core/memory.h"

namespace Kernel::Svc {

/// Used to output a message on a debug hardware unit - does nothing on a retail unit
void OutputDebugString(Core::System& system, VAddr address, u64 len) {
    if (len == 0) {
        return;
    }

    std::string str(len, '\0');
    system.Memory().ReadBlock(address, str.data(), str.size());
    LOG_DEBUG(Debug_Emulated, "{}", str);
}

void OutputDebugString32(Core::System& system, u32 address, u32 len) {
    OutputDebugString(system, address, len);
}

} // namespace Kernel::Svc
