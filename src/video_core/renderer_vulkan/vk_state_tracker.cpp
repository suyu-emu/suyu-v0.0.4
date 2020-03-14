// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstddef>
#include <iterator>

#include "common/common_types.h"
#include "core/core.h"
#include "video_core/dirty_flags.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/gpu.h"
#include "video_core/renderer_vulkan/vk_state_tracker.h"

#define OFF(field_name) MAXWELL3D_REG_INDEX(field_name)
#define NUM(field_name) (sizeof(Maxwell3D::Regs::field_name) / sizeof(u32))

namespace Vulkan {

namespace {

using namespace Dirty;
using namespace VideoCommon::Dirty;
using Tegra::Engines::Maxwell3D;
using Regs = Maxwell3D::Regs;
using Tables = Maxwell3D::DirtyState::Tables;
using Table = Maxwell3D::DirtyState::Table;
using Flags = Maxwell3D::DirtyState::Flags;

Flags MakeInvalidationFlags() {
    Flags flags{};
    flags[Viewports] = true;
    flags[Scissors] = true;
    flags[DepthBias] = true;
    flags[BlendConstants] = true;
    flags[DepthBounds] = true;
    flags[StencilProperties] = true;
    return flags;
}

void SetupDirtyViewports(Tables& tables) {
    FillBlock(tables[0], OFF(viewport_transform), NUM(viewport_transform), Viewports);
    FillBlock(tables[0], OFF(viewports), NUM(viewports), Viewports);
    tables[0][OFF(viewport_transform_enabled)] = Viewports;
}

void SetupDirtyScissors(Tables& tables) {
    FillBlock(tables[0], OFF(scissor_test), NUM(scissor_test), Scissors);
}

void SetupDirtyDepthBias(Tables& tables) {
    auto& table = tables[0];
    table[OFF(polygon_offset_units)] = DepthBias;
    table[OFF(polygon_offset_clamp)] = DepthBias;
    table[OFF(polygon_offset_factor)] = DepthBias;
}

void SetupDirtyBlendConstants(Tables& tables) {
    FillBlock(tables[0], OFF(blend_color), NUM(blend_color), BlendConstants);
}

void SetupDirtyDepthBounds(Tables& tables) {
    FillBlock(tables[0], OFF(depth_bounds), NUM(depth_bounds), DepthBounds);
}

void SetupDirtyStencilProperties(Tables& tables) {
    auto& table = tables[0];
    table[OFF(stencil_two_side_enable)] = StencilProperties;
    table[OFF(stencil_front_func_ref)] = StencilProperties;
    table[OFF(stencil_front_mask)] = StencilProperties;
    table[OFF(stencil_front_func_mask)] = StencilProperties;
    table[OFF(stencil_back_func_ref)] = StencilProperties;
    table[OFF(stencil_back_mask)] = StencilProperties;
    table[OFF(stencil_back_func_mask)] = StencilProperties;
}

} // Anonymous namespace

StateTracker::StateTracker(Core::System& system)
    : system{system}, invalidation_flags{MakeInvalidationFlags()} {}

void StateTracker::Initialize() {
    auto& dirty = system.GPU().Maxwell3D().dirty;
    auto& tables = dirty.tables;
    SetupDirtyRenderTargets(tables);
    SetupDirtyViewports(tables);
    SetupDirtyScissors(tables);
    SetupDirtyDepthBias(tables);
    SetupDirtyBlendConstants(tables);
    SetupDirtyDepthBounds(tables);
    SetupDirtyStencilProperties(tables);
}

void StateTracker::InvalidateCommandBufferState() {
    system.GPU().Maxwell3D().dirty.flags |= invalidation_flags;
}

} // namespace Vulkan
