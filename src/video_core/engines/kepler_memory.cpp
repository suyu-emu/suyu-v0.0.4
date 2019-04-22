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
    std::memcpy(&state.inner_buffer[state.write_offset], &regs.data, sub_copy_size);
    state.write_offset += sub_copy_size;
    if (is_last_call) {
        const GPUVAddr address{regs.dest.Address()};
        if (regs.exec.linear != 0) {
            memory_manager.WriteBlock(address, state.inner_buffer.data(), state.copy_size);
        } else {
            UNIMPLEMENTED_IF(regs.dest.z != 0);
            UNIMPLEMENTED_IF(regs.dest.depth != 1);
            UNIMPLEMENTED_IF(regs.dest.BlockWidth() != 1);
            UNIMPLEMENTED_IF(regs.dest.BlockDepth() != 1);
            const std::size_t dst_size = Tegra::Texture::CalculateSize(
                true, 1, regs.dest.width, regs.dest.height, 1, regs.dest.BlockHeight(), 1);
            std::vector<u8> tmp_buffer(dst_size);
            memory_manager.ReadBlock(address, tmp_buffer.data(), dst_size);
            Tegra::Texture::SwizzleKepler(regs.dest.width, regs.dest.height, regs.dest.x,
                                          regs.dest.y, regs.dest.BlockHeight(), state.copy_size,
                                          state.inner_buffer.data(), tmp_buffer.data());
            memory_manager.WriteBlock(address, tmp_buffer.data(), dst_size);
        }
        system.GPU().Maxwell3D().dirty_flags.OnMemoryWrite();
    }
}

} // namespace Tegra::Engines
