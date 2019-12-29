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

void SetupDirtyColorMasks(Tables& tables) {
    tables[0][OFF(color_mask_common)] = ColorMaskCommon;
    for (std::size_t rt = 0; rt < Regs::NumRenderTargets; ++rt) {
        const std::size_t offset = OFF(color_mask) + rt * NUM(color_mask[0]);
        FillBlock(tables[0], offset, NUM(color_mask[0]), ColorMask0 + rt);
    }

    FillBlock(tables[1], OFF(color_mask), NUM(color_mask), ColorMasks);
}

void SetupDirtyVertexFormat(Tables& tables) {
    for (std::size_t i = 0; i < Regs::NumVertexAttributes; ++i) {
        const std::size_t offset = OFF(vertex_attrib_format) + i * NUM(vertex_attrib_format[0]);
        FillBlock(tables[0], offset, NUM(vertex_attrib_format[0]), VertexFormat0 + i);
    }

    FillBlock(tables[1], OFF(vertex_attrib_format), Regs::NumVertexAttributes, VertexFormats);
}

void SetupDirtyViewports(Tables& tables) {
    for (std::size_t i = 0; i < Regs::NumViewports; ++i) {
        const std::size_t transf_offset = OFF(viewport_transform) + i * NUM(viewport_transform[0]);
        const std::size_t viewport_offset = OFF(viewports) + i * NUM(viewports[0]);

        FillBlock(tables[0], transf_offset, NUM(viewport_transform[0]), Viewport0 + i);
        FillBlock(tables[0], viewport_offset, NUM(viewports[0]), Viewport0 + i);
    }

    FillBlock(tables[1], OFF(viewport_transform), NUM(viewport_transform), Viewports);
    FillBlock(tables[1], OFF(viewports), NUM(viewports), Viewports);

    tables[0][OFF(viewport_transform_enabled)] = ViewportTransform;
    tables[1][OFF(viewport_transform_enabled)] = Viewports;
}

void SetupDirtyScissors(Tables& tables) {
    for (std::size_t i = 0; i < Regs::NumViewports; ++i) {
        const std::size_t offset = OFF(scissor_test) + i * NUM(scissor_test[0]);
        FillBlock(tables[0], offset, NUM(scissor_test[0]), Scissor0 + i);
    }
    FillBlock(tables[1], OFF(scissor_test), NUM(scissor_test), Scissors);
}

} // Anonymous namespace

StateTracker::StateTracker(Core::System& system) : system{system} {}

void StateTracker::Initialize() {
    auto& dirty = system.GPU().Maxwell3D().dirty;
    auto& tables = dirty.tables;
    SetupDirtyRenderTargets(tables);
    SetupDirtyColorMasks(tables);
    SetupDirtyViewports(tables);
    SetupDirtyScissors(tables);
    SetupDirtyVertexFormat(tables);

    auto& store = dirty.on_write_stores;
    store[RenderTargets] = true;
    store[ZetaBuffer] = true;
    for (std::size_t i = 0; i < Regs::NumRenderTargets; ++i) {
        store[ColorBuffer0 + i] = true;
    }
    store[VertexBuffers] = true;
    for (std::size_t i = 0; i < Regs::NumVertexArrays; ++i) {
        store[VertexBuffer0 + i] = true;
    }
}

} // namespace OpenGL
