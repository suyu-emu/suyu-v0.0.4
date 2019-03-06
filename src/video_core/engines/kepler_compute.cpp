// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "common/logging/log.h"
#include "video_core/engines/kepler_compute.h"
#include "video_core/memory_manager.h"

namespace Tegra::Engines {

KeplerCompute::KeplerCompute(MemoryManager& memory_manager) : memory_manager{memory_manager} {}

KeplerCompute::~KeplerCompute() = default;

void KeplerCompute::CallMethod(const GPU::MethodCall& method_call) {
    ASSERT_MSG(method_call.method < Regs::NUM_REGS,
               "Invalid KeplerCompute register, increase the size of the Regs structure");

    regs.reg_array[method_call.method] = method_call.argument;

    switch (method_call.method) {
    case KEPLER_COMPUTE_REG_INDEX(launch):
        // Abort execution since compute shaders can be used to alter game memory (e.g. CUDA
        // kernels)
        UNREACHABLE_MSG("Compute shaders are not implemented");
        break;
    default:
        break;
    }
}

} // namespace Tegra::Engines
