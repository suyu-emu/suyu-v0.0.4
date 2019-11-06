// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "common/logging/log.h"
#include "video_core/engines/fermi_2d.h"
#include "video_core/memory_manager.h"
#include "video_core/rasterizer_interface.h"

namespace Tegra::Engines {

Fermi2D::Fermi2D(VideoCore::RasterizerInterface& rasterizer) : rasterizer{rasterizer} {}

void Fermi2D::CallMethod(const GPU::MethodCall& method_call) {
    ASSERT_MSG(method_call.method < Regs::NUM_REGS,
               "Invalid Fermi2D register, increase the size of the Regs structure");

    regs.reg_array[method_call.method] = method_call.argument;

    switch (method_call.method) {
    // Trigger the surface copy on the last register write. This is blit_src_y, but this is 64-bit,
    // so trigger on the second 32-bit write.
    case FERMI2D_REG_INDEX(blit_src_y) + 1: {
        HandleSurfaceCopy();
        break;
    }
    }
}

std::pair<u32, u32> DelimitLine(u32 src_1, u32 src_2, u32 dst_1, u32 dst_2, u32 src_line) {
    const u32 line_a = src_2 - src_1;
    const u32 line_b = dst_2 - dst_1;
    const u32 excess = std::max<s32>(0, line_a - src_line + src_1);
    return {line_b - (excess * line_b) / line_a, excess};
}

void Fermi2D::HandleSurfaceCopy() {
    LOG_DEBUG(HW_GPU, "Requested a surface copy with operation {}",
              static_cast<u32>(regs.operation));

    // TODO(Subv): Only raw copies are implemented.
    ASSERT(regs.operation == Operation::SrcCopy);

    const u32 src_blit_x1{static_cast<u32>(regs.blit_src_x >> 32)};
    const u32 src_blit_y1{static_cast<u32>(regs.blit_src_y >> 32)};
    u32 src_blit_x2, src_blit_y2;
    if (regs.blit_control.origin == Origin::Corner) {
        src_blit_x2 =
            static_cast<u32>((regs.blit_src_x + (regs.blit_du_dx * regs.blit_dst_width)) >> 32);
        src_blit_y2 =
            static_cast<u32>((regs.blit_src_y + (regs.blit_dv_dy * regs.blit_dst_height)) >> 32);
    } else {
        src_blit_x2 = static_cast<u32>((regs.blit_src_x >> 32) + regs.blit_dst_width);
        src_blit_y2 = static_cast<u32>((regs.blit_src_y >> 32) + regs.blit_dst_height);
    }
    u32 dst_blit_x2 = regs.blit_dst_x + regs.blit_dst_width;
    u32 dst_blit_y2 = regs.blit_dst_y + regs.blit_dst_height;
    const auto [new_dst_w, src_excess_x] =
        DelimitLine(src_blit_x1, src_blit_x2, regs.blit_dst_x, dst_blit_x2, regs.src.width);
    const auto [new_dst_h, src_excess_y] =
        DelimitLine(src_blit_y1, src_blit_y2, regs.blit_dst_y, dst_blit_y2, regs.src.height);
    dst_blit_x2 = new_dst_w + regs.blit_dst_x;
    src_blit_x2 = src_blit_x2 - src_excess_x;
    dst_blit_y2 = new_dst_h + regs.blit_dst_y;
    src_blit_y2 = src_blit_y2 - src_excess_y;
    const auto [new_src_w, dst_excess_x] =
        DelimitLine(regs.blit_dst_x, dst_blit_x2, src_blit_x1, src_blit_x2, regs.dst.width);
    const auto [new_src_h, dst_excess_y] =
        DelimitLine(regs.blit_dst_y, dst_blit_y2, src_blit_y1, src_blit_y2, regs.dst.height);
    src_blit_x2 = new_src_w + src_blit_x1;
    dst_blit_x2 = dst_blit_x2 - dst_excess_x;
    src_blit_y2 = new_src_h + src_blit_y1;
    dst_blit_y2 = dst_blit_y2 - dst_excess_y;
    const Common::Rectangle<u32> src_rect{src_blit_x1, src_blit_y1, src_blit_x2, src_blit_y2};
    const Common::Rectangle<u32> dst_rect{regs.blit_dst_x, regs.blit_dst_y, dst_blit_x2,
                                          dst_blit_y2};
    Config copy_config;
    copy_config.operation = regs.operation;
    copy_config.filter = regs.blit_control.filter;
    copy_config.src_rect = src_rect;
    copy_config.dst_rect = dst_rect;

    if (!rasterizer.AccelerateSurfaceCopy(regs.src, regs.dst, copy_config)) {
        UNIMPLEMENTED();
    }
}

} // namespace Tegra::Engines
