// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <cstddef>

#include "common/common_types.h"
#include "video_core/dirty_flags.h"

#define OFF(field_name) MAXWELL3D_REG_INDEX(field_name)
#define NUM(field_name) (sizeof(::Tegra::Engines::Maxwell3D::Regs::field_name) / (sizeof(u32)))

namespace VideoCommon::Dirty {

using Tegra::Engines::Maxwell3D;

void SetupDirtyRenderTargets(Tegra::Engines::Maxwell3D::DirtyState::Tables& tables) {
    static constexpr std::size_t num_per_rt = NUM(rt[0]);
    static constexpr std::size_t begin = OFF(rt);
    static constexpr std::size_t num = num_per_rt * Maxwell3D::Regs::NumRenderTargets;
    for (std::size_t rt = 0; rt < Maxwell3D::Regs::NumRenderTargets; ++rt) {
        FillBlock(tables[0], begin + rt * num_per_rt, num_per_rt, ColorBuffer0 + rt);
    }
    FillBlock(tables[1], begin, num, RenderTargets);

    static constexpr std::array zeta_flags{ZetaBuffer, RenderTargets};
    for (std::size_t i = 0; i < std::size(zeta_flags); ++i) {
        const u8 flag = zeta_flags[i];
        auto& table = tables[i];
        table[OFF(zeta_enable)] = flag;
        table[OFF(zeta_width)] = flag;
        table[OFF(zeta_height)] = flag;
        FillBlock(table, OFF(zeta), NUM(zeta), flag);
    }
}

} // namespace VideoCommon::Dirty
