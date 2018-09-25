// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "core/core.h"
#include "video_core/engines/maxwell_compute.h"

namespace Tegra {
namespace Engines {

void MaxwellCompute::WriteReg(u32 method, u32 value) {
    ASSERT_MSG(method < Regs::NUM_REGS,
               "Invalid MaxwellCompute register, increase the size of the Regs structure");

    regs.reg_array[method] = value;

    switch (method) {
    case MAXWELL_COMPUTE_REG_INDEX(compute): {
        LOG_CRITICAL(HW_GPU, "Compute shaders are not implemented");
        UNREACHABLE();
        break;
    }
    default:
        break;
    }
}

} // namespace Engines
} // namespace Tegra
