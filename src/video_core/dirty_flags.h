// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2019 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <cstddef>
#include <iterator>

#include "common/common_types.h"
#include "video_core/engines/maxwell_3d.h"

namespace VideoCommon::Dirty {

enum : u8 {
    NullEntry = 0,

    Descriptors,

    RenderTargets,
    RenderTargetControl,
    ColorBuffer0,
    ColorBuffer1,
    ColorBuffer2,
    ColorBuffer3,
    ColorBuffer4,
    ColorBuffer5,
    ColorBuffer6,
    ColorBuffer7,
    ZetaBuffer,
    RescaleViewports,
    RescaleScissors,

    VertexBuffers,
    VertexBuffer0,
    VertexBuffer31 = VertexBuffer0 + 31,

    IndexBuffer,

    Shaders,

    // Special entries
    DepthBiasGlobal,

    LastCommonEntry,
};

constexpr std::pair<u8, u8> GetDirtyFlagsForMethod(u32 method) {
    const u32 OFF_VERTEX_STREAMS = 0x2C0;
    const u32 OFF_VERTEX_STREAM_LIMITS = 0x2F8;
    const u32 OFF_INDEX_BUFFER = 0x460;
    const u32 OFF_TEX_HEADER = 0x800;
    const u32 OFF_TEX_SAMPLER = 0xA00;
    const u32 OFF_RT = 0xE00;
    const u32 OFF_SURFACE_CLIP = 0xE38;
    const u32 OFF_RT_CONTROL = 0xE40;
    const u32 OFF_ZETA_ENABLE = 0xE4C;
    const u32 OFF_ZETA_SIZE_WIDTH = 0xE50;
    const u32 OFF_ZETA_SIZE_HEIGHT = 0xE54;
    const u32 OFF_ZETA = 0xE60;
    const u32 OFF_PIPELINES = 0x1D00;

    if (method >= OFF_VERTEX_STREAMS && method < OFF_VERTEX_STREAMS + 96) {
        const u32 buffer_idx = (method - OFF_VERTEX_STREAMS) / 3;
        return {static_cast<u8>(VertexBuffer0 + buffer_idx), VertexBuffers};
    }

    if (method >= OFF_VERTEX_STREAM_LIMITS && method < OFF_VERTEX_STREAM_LIMITS + 32) {
        const u32 buffer_idx = method - OFF_VERTEX_STREAM_LIMITS;
        return {static_cast<u8>(VertexBuffer0 + buffer_idx), VertexBuffers};
    }

    if (method == OFF_INDEX_BUFFER || (method > OFF_INDEX_BUFFER && method < OFF_INDEX_BUFFER + 3)) {
        return {IndexBuffer, NullEntry};
    }

    if (method >= OFF_TEX_HEADER && method < OFF_TEX_HEADER + 256) {
        return {Descriptors, NullEntry};
    }

    if (method >= OFF_TEX_SAMPLER && method < OFF_TEX_SAMPLER + 256) {
        return {Descriptors, NullEntry};
    }

    if (method >= OFF_RT && method < OFF_RT + 64) {
        const u32 rt_idx = (method - OFF_RT) / 8;
        return {static_cast<u8>(ColorBuffer0 + rt_idx), RenderTargets};
    }

    if (method == OFF_SURFACE_CLIP || (method > OFF_SURFACE_CLIP && method < OFF_SURFACE_CLIP + 4)) {
        return {RenderTargets, NullEntry};
    }

    if (method == OFF_RT_CONTROL) {
        return {RenderTargets, RenderTargetControl};
    }

    if (method == OFF_ZETA_ENABLE || method == OFF_ZETA_SIZE_WIDTH || method == OFF_ZETA_SIZE_HEIGHT) {
        return {ZetaBuffer, RenderTargets};
    }

    if (method >= OFF_ZETA && method < OFF_ZETA + 8) {
        return {ZetaBuffer, RenderTargets};
    }

    if (method >= OFF_PIPELINES && method < OFF_PIPELINES + 1024) {
        return {Shaders, NullEntry};
    }

    return {NullEntry, NullEntry};
}

template <typename Integer>
void FillBlock(Tegra::Engines::Maxwell3D::DirtyState::Table& table, std::size_t begin,
               std::size_t num, Integer dirty_index) {
    const auto it = std::begin(table) + begin;
    std::fill(it, it + num, static_cast<u8>(dirty_index));
}

template <typename Integer1, typename Integer2>
void FillBlock(Tegra::Engines::Maxwell3D::DirtyState::Tables& tables, std::size_t begin,
               std::size_t num, Integer1 index_a, Integer2 index_b) {
    FillBlock(tables[0], begin, num, index_a);
    FillBlock(tables[1], begin, num, index_b);
}

void SetupDirtyFlags(Tegra::Engines::Maxwell3D::DirtyState::Tables& tables);

} // namespace VideoCommon::Dirty
