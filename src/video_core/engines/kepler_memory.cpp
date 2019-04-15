// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "video_core/engines/kepler_memory.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/memory_manager.h"
#include "video_core/rasterizer_interface.h"
#include "video_core/renderer_base.h"
#include "video_core/textures/convert.h"
#include "video_core/textures/decoders.h"

namespace Tegra::Engines {

KeplerMemory::KeplerMemory(Core::System& system, VideoCore::RasterizerInterface& rasterizer,
                           MemoryManager& memory_manager)
    : system{system}, rasterizer{rasterizer}, memory_manager{memory_manager} {}

KeplerMemory::~KeplerMemory() = default;

void KeplerMemory::CallMethod(const GPU::MethodCall& method_call) {
    ASSERT_MSG(method_call.method < Regs::NUM_REGS,
               "Invalid KeplerMemory register, increase the size of the Regs structure");

    regs.reg_array[method_call.method] = method_call.argument;

    switch (method_call.method) {
    case KEPLERMEMORY_REG_INDEX(exec): {
        ProcessExec();
        break;
    }
    case KEPLERMEMORY_REG_INDEX(data): {
        ProcessData(method_call.argument, method_call.IsLastCall());
        break;
    }
    }
}

void KeplerMemory::ProcessExec() {
    state.write_offset = 0;
    state.copy_size = regs.line_length_in * regs.line_count;
    state.inner_buffer.resize(state.copy_size);
}

void KeplerMemory::ProcessData(u32 data, bool is_last_call) {
    const u32 sub_copy_size = std::min(4U, state.copy_size - state.write_offset);
    std::memcpy(&state.inner_buffer[state.write_offset], &data, sub_copy_size);
    state.write_offset += sub_copy_size;
    if (is_last_call) {
        UNIMPLEMENTED_IF_MSG(regs.exec.linear == 0, "Block Linear Copy is not implemented");
        if (regs.exec.linear != 0) {
            const GPUVAddr address{regs.dest.Address()};
            const auto host_ptr = memory_manager.GetPointer(address);
            // We have to invalidate the destination region to evict any outdated surfaces from the
            // cache. We do this before actually writing the new data because the destination
            // address might contain a dirty surface that will have to be written back to memory.

            rasterizer.InvalidateRegion(ToCacheAddr(host_ptr), state.copy_size);
            std::memcpy(host_ptr, state.inner_buffer.data(), state.copy_size);
            system.GPU().Maxwell3D().dirty_flags.OnMemoryWrite();
        }
    }
}

} // namespace Tegra::Engines
