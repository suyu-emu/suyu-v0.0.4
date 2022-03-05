// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <vector>
#include "common/assert.h"
#include "common/scope_exit.h"
#include "video_core/dirty_flags.h"
#include "video_core/engines/draw_manager.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/macro/macro.h"
#include "video_core/macro/macro_hle.h"
#include "video_core/memory_manager.h"
#include "video_core/rasterizer_interface.h"

namespace Tegra {
namespace {

bool IsTopologySafe(Tegra::Engines::Maxwell3D::Regs::PrimitiveTopology topology) {
    switch (topology) {
    case Tegra::Engines::Maxwell3D::Regs::PrimitiveTopology::Points:
    case Tegra::Engines::Maxwell3D::Regs::PrimitiveTopology::Lines:
    case Tegra::Engines::Maxwell3D::Regs::PrimitiveTopology::LineLoop:
    case Tegra::Engines::Maxwell3D::Regs::PrimitiveTopology::LineStrip:
    case Tegra::Engines::Maxwell3D::Regs::PrimitiveTopology::Triangles:
    case Tegra::Engines::Maxwell3D::Regs::PrimitiveTopology::TriangleStrip:
    case Tegra::Engines::Maxwell3D::Regs::PrimitiveTopology::TriangleFan:
    case Tegra::Engines::Maxwell3D::Regs::PrimitiveTopology::LinesAdjacency:
    case Tegra::Engines::Maxwell3D::Regs::PrimitiveTopology::LineStripAdjacency:
    case Tegra::Engines::Maxwell3D::Regs::PrimitiveTopology::TrianglesAdjacency:
    case Tegra::Engines::Maxwell3D::Regs::PrimitiveTopology::TriangleStripAdjacency:
    case Tegra::Engines::Maxwell3D::Regs::PrimitiveTopology::Patches:
        return true;
    case Tegra::Engines::Maxwell3D::Regs::PrimitiveTopology::Quads:
    case Tegra::Engines::Maxwell3D::Regs::PrimitiveTopology::QuadStrip:
    case Tegra::Engines::Maxwell3D::Regs::PrimitiveTopology::Polygon:
    default:
        return false;
    }
}

class HLEMacroImpl : public CachedMacro {
public:
    explicit HLEMacroImpl(Engines::Maxwell3D& maxwell3d_) : maxwell3d{maxwell3d_} {}

protected:
    void advanceCheck() {
        current_value = (current_value + 1) % fibonacci_post;
        check_limit = current_value == 0;
        if (check_limit) {
            const u32 new_fibonacci = fibonacci_pre + fibonacci_post;
            fibonacci_pre = fibonacci_post;
            fibonacci_post = new_fibonacci;
        }
    }

    Engines::Maxwell3D& maxwell3d;
    u32 fibonacci_pre{89};
    u32 fibonacci_post{144};
    u32 current_value{fibonacci_post - 1};
    bool check_limit{};
};

class HLE_771BB18C62444DA0 final : public HLEMacroImpl {
public:
    explicit HLE_771BB18C62444DA0(Engines::Maxwell3D& maxwell3d_) : HLEMacroImpl(maxwell3d_) {}

    void Execute(const std::vector<u32>& parameters, [[maybe_unused]] u32 method) override {
        maxwell3d.RefreshParameters();
        const u32 instance_count = parameters[2] & maxwell3d.GetRegisterValue(0xD1B);
        maxwell3d.dirty.flags[VideoCommon::Dirty::IndexBuffer] = true;
        maxwell3d.draw_manager->DrawIndex(
            static_cast<Tegra::Engines::Maxwell3D::Regs::PrimitiveTopology>(parameters[0] &
                                                                            0x3ffffff),
            parameters[4], parameters[1], parameters[3], parameters[5], instance_count);
    }
};

class HLE_DrawArraysIndirect final : public HLEMacroImpl {
public:
    explicit HLE_DrawArraysIndirect(Engines::Maxwell3D& maxwell3d_, bool extended_ = false)
        : HLEMacroImpl(maxwell3d_), extended(extended_) {}

    void Execute(const std::vector<u32>& parameters, [[maybe_unused]] u32 method) override {
        auto topology =
            static_cast<Tegra::Engines::Maxwell3D::Regs::PrimitiveTopology>(parameters[0]);
        if (!IsTopologySafe(topology)) {
            Fallback(parameters);
            return;
        }

        auto& params = maxwell3d.draw_manager->GetIndirectParams();
        params.is_indexed = false;
        params.include_count = false;
        params.count_start_address = 0;
        params.indirect_start_address = maxwell3d.getMacroAddress(1);
        params.buffer_size = 4 * sizeof(u32);
        params.max_draw_counts = 1;
        params.stride = 0;

        if (extended) {
            maxwell3d.CallMethod(0x8e3, 0x640, true);
            maxwell3d.CallMethod(0x8e4, parameters[4], true);
        }

        maxwell3d.draw_manager->DrawArrayIndirect(topology);

        if (extended) {
            maxwell3d.CallMethod(0x8e3, 0x640, true);
            maxwell3d.CallMethod(0x8e4, 0, true);
        }
        maxwell3d.regs.vertex_buffer.first = 0;
        maxwell3d.regs.vertex_buffer.count = 0;
    }

private:
    void Fallback(const std::vector<u32>& parameters) {
        SCOPE_EXIT({
            if (extended) {
                maxwell3d.CallMethod(0x8e3, 0x640, true);
                maxwell3d.CallMethod(0x8e4, 0, true);
            }
        });
        maxwell3d.RefreshParameters();
        const u32 instance_count = (maxwell3d.GetRegisterValue(0xD1B) & parameters[2]);

        const u32 vertex_first = parameters[3];
        const u32 vertex_count = parameters[1];

        if (maxwell3d.GetMaxCurrentVertices() < vertex_first + vertex_count) {
            ASSERT_MSG(false, "Faulty draw!");
            return;
        }

        const u32 base_instance = parameters[4];
        if (extended) {
            maxwell3d.CallMethod(0x8e3, 0x640, true);
            maxwell3d.CallMethod(0x8e4, base_instance, true);
        }

        maxwell3d.draw_manager->DrawArray(
            static_cast<Tegra::Engines::Maxwell3D::Regs::PrimitiveTopology>(parameters[0]),
            vertex_first, vertex_count, base_instance, instance_count);
    }

    bool extended;
};

class HLE_DrawIndexedIndirect final : public HLEMacroImpl {
public:
    explicit HLE_DrawIndexedIndirect(Engines::Maxwell3D& maxwell3d_) : HLEMacroImpl(maxwell3d_) {}

    void Execute(const std::vector<u32>& parameters, [[maybe_unused]] u32 method) override {
        auto topology =
            static_cast<Tegra::Engines::Maxwell3D::Regs::PrimitiveTopology>(parameters[0]);
        if (!IsTopologySafe(topology)) {
            Fallback(parameters);
            return;
        }

        advanceCheck();
        if (check_limit) {
            maxwell3d.RefreshParameters();
            minimum_limit = std::max(parameters[3], minimum_limit);
        }

        const u32 base_vertex = parameters[8];
        const u32 base_instance = parameters[9];
        maxwell3d.regs.vertex_id_base = base_vertex;
        maxwell3d.CallMethod(0x8e3, 0x640, true);
        maxwell3d.CallMethod(0x8e4, base_vertex, true);
        maxwell3d.CallMethod(0x8e5, base_instance, true);
        auto& params = maxwell3d.draw_manager->GetIndirectParams();
        params.is_indexed = true;
        params.include_count = false;
        params.count_start_address = 0;
        params.indirect_start_address = maxwell3d.getMacroAddress(1);
        params.buffer_size = 5 * sizeof(u32);
        params.max_draw_counts = 1;
        params.stride = 0;
        maxwell3d.dirty.flags[VideoCommon::Dirty::IndexBuffer] = true;
        maxwell3d.draw_manager->DrawIndexedIndirect(topology, 0, minimum_limit);
        maxwell3d.CallMethod(0x8e3, 0x640, true);
        maxwell3d.CallMethod(0x8e4, 0x0, true);
        maxwell3d.CallMethod(0x8e5, 0x0, true);
    }

private:
    void Fallback(const std::vector<u32>& parameters) {
        maxwell3d.RefreshParameters();
        const u32 instance_count = (maxwell3d.GetRegisterValue(0xD1B) & parameters[2]);
        const u32 element_base = parameters[4];
        const u32 base_instance = parameters[5];
        maxwell3d.regs.vertex_id_base = element_base;
        maxwell3d.dirty.flags[VideoCommon::Dirty::IndexBuffer] = true;
        maxwell3d.CallMethod(0x8e3, 0x640, true);
        maxwell3d.CallMethod(0x8e4, element_base, true);
        maxwell3d.CallMethod(0x8e5, base_instance, true);

        maxwell3d.draw_manager->DrawIndex(
            static_cast<Tegra::Engines::Maxwell3D::Regs::PrimitiveTopology>(parameters[0]),
            parameters[3], parameters[1], element_base, base_instance, instance_count);

        maxwell3d.regs.vertex_id_base = 0x0;
        maxwell3d.CallMethod(0x8e3, 0x640, true);
        maxwell3d.CallMethod(0x8e4, 0x0, true);
        maxwell3d.CallMethod(0x8e5, 0x0, true);
    }

    u32 minimum_limit{1 << 18};
};

class HLE_MultiLayerClear final : public HLEMacroImpl {
public:
    explicit HLE_MultiLayerClear(Engines::Maxwell3D& maxwell3d_) : HLEMacroImpl(maxwell3d_) {}

    void Execute(const std::vector<u32>& parameters, [[maybe_unused]] u32 method) override {
        maxwell3d.RefreshParameters();
        ASSERT(parameters.size() == 1);

        const Engines::Maxwell3D::Regs::ClearSurface clear_params{parameters[0]};
        const u32 rt_index = clear_params.RT;
        const u32 num_layers = maxwell3d.regs.rt[rt_index].depth;
        ASSERT(clear_params.layer == 0);

        maxwell3d.regs.clear_surface.raw = clear_params.raw;
        maxwell3d.draw_manager->Clear(num_layers);
    }
};

class HLE_MultiDrawIndexedIndirectCount final : public HLEMacroImpl {
public:
    explicit HLE_MultiDrawIndexedIndirectCount(Engines::Maxwell3D& maxwell3d_)
        : HLEMacroImpl(maxwell3d_) {}

    void Execute(const std::vector<u32>& parameters, [[maybe_unused]] u32 method) override {
        const auto topology =
            static_cast<Tegra::Engines::Maxwell3D::Regs::PrimitiveTopology>(parameters[2]);
        if (!IsTopologySafe(topology)) {
            Fallback(parameters);
            return;
        }

        advanceCheck();
        if (check_limit) {
            maxwell3d.RefreshParameters();
        }
        const u32 start_indirect = parameters[0];
        const u32 end_indirect = parameters[1];
        if (start_indirect >= end_indirect) {
            // Nothing to do.
            return;
        }

        maxwell3d.regs.draw.topology.Assign(topology);
        const u32 padding = parameters[3]; // padding is in words

        // size of each indirect segment
        const u32 indirect_words = 5 + padding;
        const u32 stride = indirect_words * sizeof(u32);
        const std::size_t draw_count = end_indirect - start_indirect;
        u32 lowest_first = std::numeric_limits<u32>::max();
        u32 highest_limit = std::numeric_limits<u32>::min();
        for (std::size_t index = 0; index < draw_count; index++) {
            const std::size_t base = index * indirect_words + 5;
            const u32 count = parameters[base];
            const u32 first_index = parameters[base + 2];
            lowest_first = std::min(lowest_first, first_index);
            highest_limit = std::max(highest_limit, first_index + count);
        }
        if (check_limit) {
            minimum_limit = std::max(highest_limit, minimum_limit);
        }

        maxwell3d.regs.index_buffer.first = 0;
        maxwell3d.regs.index_buffer.count = std::max(highest_limit, minimum_limit);
        maxwell3d.dirty.flags[VideoCommon::Dirty::IndexBuffer] = true;
        auto& params = maxwell3d.draw_manager->GetIndirectParams();
        params.is_indexed = true;
        params.include_count = true;
        params.count_start_address = maxwell3d.getMacroAddress(4);
        params.indirect_start_address = maxwell3d.getMacroAddress(5);
        params.buffer_size = stride * draw_count;
        params.max_draw_counts = draw_count;
        params.stride = stride;
        maxwell3d.dirty.flags[VideoCommon::Dirty::IndexBuffer] = true;
        maxwell3d.draw_manager->DrawIndexedIndirect(topology, 0, highest_limit);
    }

private:
    void Fallback(const std::vector<u32>& parameters) {
        SCOPE_EXIT({
            // Clean everything.
            // Clean everything.
            maxwell3d.regs.vertex_id_base = 0x0;
            maxwell3d.CallMethod(0x8e3, 0x640, true);
            maxwell3d.CallMethod(0x8e4, 0x0, true);
            maxwell3d.CallMethod(0x8e5, 0x0, true);
        });
        maxwell3d.RefreshParameters();
        const u32 start_indirect = parameters[0];
        const u32 end_indirect = parameters[1];
        if (start_indirect >= end_indirect) {
            // Nothing to do.
            return;
        }
        const auto topology =
            static_cast<Tegra::Engines::Maxwell3D::Regs::PrimitiveTopology>(parameters[2]);
        maxwell3d.regs.draw.topology.Assign(topology);
        const u32 padding = parameters[3];
        const std::size_t max_draws = parameters[4];

        const u32 indirect_words = 5 + padding;
        const std::size_t first_draw = start_indirect;
        const std::size_t effective_draws = end_indirect - start_indirect;
        const std::size_t last_draw = start_indirect + std::min(effective_draws, max_draws);

        for (std::size_t index = first_draw; index < last_draw; index++) {
            const std::size_t base = index * indirect_words + 5;
            const u32 base_vertex = parameters[base + 3];
            const u32 base_instance = parameters[base + 4];
            maxwell3d.regs.vertex_id_base = base_vertex;
            maxwell3d.CallMethod(0x8e3, 0x640, true);
            maxwell3d.CallMethod(0x8e4, base_vertex, true);
            maxwell3d.CallMethod(0x8e5, base_instance, true);
            maxwell3d.dirty.flags[VideoCommon::Dirty::IndexBuffer] = true;
            maxwell3d.draw_manager->DrawIndex(topology, parameters[base + 2], parameters[base],
                                              base_vertex, base_instance, parameters[base + 1]);
        }
    }

    u32 minimum_limit{1 << 12};
};

} // Anonymous namespace

HLEMacro::HLEMacro(Engines::Maxwell3D& maxwell3d_) : maxwell3d{maxwell3d_} {
    builders.emplace(0x771BB18C62444DA0ULL,
                     std::function<std::unique_ptr<CachedMacro>(Engines::Maxwell3D&)>(
                         [](Engines::Maxwell3D& maxwell3d) -> std::unique_ptr<CachedMacro> {
                             return std::make_unique<HLE_771BB18C62444DA0>(maxwell3d);
                         }));
    builders.emplace(0x0D61FC9FAAC9FCADULL,
                     std::function<std::unique_ptr<CachedMacro>(Engines::Maxwell3D&)>(
                         [](Engines::Maxwell3D& maxwell3d) -> std::unique_ptr<CachedMacro> {
                             return std::make_unique<HLE_DrawArraysIndirect>(maxwell3d);
                         }));
    builders.emplace(0x8A4D173EB99A8603ULL,
                     std::function<std::unique_ptr<CachedMacro>(Engines::Maxwell3D&)>(
                         [](Engines::Maxwell3D& maxwell3d) -> std::unique_ptr<CachedMacro> {
                             return std::make_unique<HLE_DrawArraysIndirect>(maxwell3d, true);
                         }));
    builders.emplace(0x0217920100488FF7ULL,
                     std::function<std::unique_ptr<CachedMacro>(Engines::Maxwell3D&)>(
                         [](Engines::Maxwell3D& maxwell3d) -> std::unique_ptr<CachedMacro> {
                             return std::make_unique<HLE_DrawIndexedIndirect>(maxwell3d);
                         }));
    builders.emplace(0x3F5E74B9C9A50164ULL,
                     std::function<std::unique_ptr<CachedMacro>(Engines::Maxwell3D&)>(
                         [](Engines::Maxwell3D& maxwell3d) -> std::unique_ptr<CachedMacro> {
                             return std::make_unique<HLE_MultiDrawIndexedIndirectCount>(maxwell3d);
                         }));
    builders.emplace(0xEAD26C3E2109B06BULL,
                     std::function<std::unique_ptr<CachedMacro>(Engines::Maxwell3D&)>(
                         [](Engines::Maxwell3D& maxwell3d) -> std::unique_ptr<CachedMacro> {
                             return std::make_unique<HLE_MultiLayerClear>(maxwell3d);
                         }));
}

HLEMacro::~HLEMacro() = default;

std::unique_ptr<CachedMacro> HLEMacro::GetHLEProgram(u64 hash) const {
    const auto it = builders.find(hash);
    if (it == builders.end()) {
        return nullptr;
    }
    return it->second(maxwell3d);
}

} // namespace Tegra
