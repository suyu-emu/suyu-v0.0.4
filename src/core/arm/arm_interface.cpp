// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_types.h"
#include "common/logging/log.h"
#include "core/arm/arm_interface.h"
#include "core/memory.h"

namespace Core {
void ARM_Interface::LogBacktrace() const {
    VAddr fp = GetReg(29);
    VAddr lr = GetReg(30);
    const VAddr sp = GetReg(13);
    const VAddr pc = GetPC();

    LOG_ERROR(Core_ARM, "Backtrace, sp={:016X}, pc={:016X}", sp, pc);
    while (true) {
        LOG_ERROR(Core_ARM, "{:016X}", lr);
        if (!fp) {
            break;
        }
        lr = Memory::Read64(fp + 8) - 4;
        fp = Memory::Read64(fp);
    }
}
} // namespace Core
