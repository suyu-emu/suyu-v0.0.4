// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "arm_interface.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "core/memory.h"

namespace Core {
void ARM_Interface::LogBacktrace() {
    VAddr fp = GetReg(29);
    VAddr lr = GetReg(30);
    VAddr sp = GetReg(13);
    VAddr pc = GetPC();
    LOG_ERROR(Core_ARM, "Backtrace, sp={:016X}, pc={:016X}", sp, pc);
    for (;;) {
        LOG_ERROR(Core_ARM, "{:016X}", lr);
        if (!fp) {
            break;
        }
        lr = Memory::Read64(fp + 8) - 4;
        fp = Memory::Read64(fp);
    }
}
}; // namespace Core
