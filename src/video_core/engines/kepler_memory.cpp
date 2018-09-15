// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "core/memory.h"
#include "video_core/engines/kepler_memory.h"

namespace Tegra::Engines {

KeplerMemory::KeplerMemory(MemoryManager& memory_manager) : memory_manager(memory_manager) {}
KeplerMemory::~KeplerMemory() = default;

void KeplerMemory::WriteReg(u32 method, u32 value) {
    ASSERT_MSG(method < Regs::NUM_REGS,
               "Invalid KeplerMemory register, increase the size of the Regs structure");

    regs.reg_array[method] = value;

    switch (method) {
    case KEPLERMEMORY_REG_INDEX(exec): {
        state.write_offset = 0;
        break;
    }
    case KEPLERMEMORY_REG_INDEX(data): {
        ProcessData(value);
        break;
    }
    }
}

void KeplerMemory::ProcessData(u32 data) {
    ASSERT_MSG(regs.exec.linear, "Non-linear uploads are not supported");
    ASSERT(regs.dest.x == 0 && regs.dest.y == 0 && regs.dest.z == 0);

    GPUVAddr address = regs.dest.Address();
    VAddr dest_address =
        *memory_manager.GpuToCpuAddress(address + state.write_offset * sizeof(u32));

    Memory::Write32(dest_address, data);

    state.write_offset++;
}

} // namespace Tegra::Engines
