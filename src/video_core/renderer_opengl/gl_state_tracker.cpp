// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <cstddef>
#include <type_traits>

#include "common/common_types.h"
#include "core/core.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/gpu.h"
#include "video_core/renderer_opengl/gl_state_tracker.h"

#define OFF(field_name) MAXWELL3D_REG_INDEX(field_name)
#define NUM(field_name) (sizeof(Maxwell3D::Regs::field_name) / sizeof(u32))

namespace OpenGL {

namespace {

using namespace Dirty;
using namespace VideoCommon::Dirty;
using Tegra::Engines::Maxwell3D;
using Regs = Maxwell3D::Regs;
using Dirty = std::remove_reference_t<decltype(Maxwell3D::dirty)>;
using Tables = std::remove_reference_t<decltype(Maxwell3D::dirty.tables)>;
using Table = std::remove_reference_t<decltype(Maxwell3D::dirty.tables[0])>;

template <typename Integer>
void FillBlock(Table& table, std::size_t begin, std::size_t num, Integer dirty_index) {
    const auto it = std::begin(table) + begin;
    std::fill(it, it + num, static_cast<u8>(dirty_index));
}

template <typename Integer1, typename Integer2>
void FillBlock(Tables& tables, std::size_t begin, std::size_t num, Integer1 index_a,
               Integer2 index_b) {
    FillBlock(tables[0], begin, num, index_a);
    FillBlock(tables[1], begin, num, index_b);
}

void SetupDirtyRenderTargets(Tables& tables) {
    static constexpr std::size_t num_per_rt = NUM(rt[0]);
    static constexpr std::size_t begin = OFF(rt);
    static constexpr std::size_t num = num_per_rt * Regs::NumRenderTargets;
    for (std::size_t rt = 0; rt < Regs::NumRenderTargets; ++rt) {
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

} // Anonymous namespace

StateTracker::StateTracker(Core::System& system) : system{system} {}

void StateTracker::Initialize() {
    auto& dirty = system.GPU().Maxwell3D().dirty;
    std::size_t entry_index = 0;
    const auto AddEntry = [&dirty, &entry_index](std::size_t dirty_register) {
        dirty.on_write_stores[entry_index++] = static_cast<u8>(dirty_register);
    };

    AddEntry(RenderTargets);
    for (std::size_t i = 0; i < Regs::NumRenderTargets; ++i) {
        AddEntry(ColorBuffer0 + i);
    }
    AddEntry(ZetaBuffer);

    auto& tables = dirty.tables;
    SetupDirtyRenderTargets(tables);
}

} // namespace OpenGL
