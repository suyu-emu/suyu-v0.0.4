// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <type_traits>

#include "common/common_types.h"

#include "video_core/engines/maxwell_3d.h"
#include "video_core/surface.h"

namespace Vulkan {

using Maxwell = Tegra::Engines::Maxwell3D::Regs;

// TODO(Rodrigo): Optimize this structure.

struct FixedPipelineState {
    using PixelFormat = VideoCore::Surface::PixelFormat;

    struct VertexBinding {
        constexpr VertexBinding(u32 index, u32 stride, u32 divisor)
            : index{index}, stride{stride}, divisor{divisor} {}
        VertexBinding() = default;

        u32 index;
        u32 stride;
        u32 divisor;

        std::size_t Hash() const noexcept;

        bool operator==(const VertexBinding& rhs) const noexcept;

        bool operator!=(const VertexBinding& rhs) const noexcept {
            return !operator==(rhs);
        }
    };

    struct VertexAttribute {
        constexpr VertexAttribute(u32 index, u32 buffer, Maxwell::VertexAttribute::Type type,
                                  Maxwell::VertexAttribute::Size size, u32 offset)
            : index{index}, buffer{buffer}, type{type}, size{size}, offset{offset} {}
        VertexAttribute() = default;

        u32 index;
        u32 buffer;
        Maxwell::VertexAttribute::Type type;
        Maxwell::VertexAttribute::Size size;
        u32 offset;

        std::size_t Hash() const noexcept;

        bool operator==(const VertexAttribute& rhs) const noexcept;

        bool operator!=(const VertexAttribute& rhs) const noexcept {
            return !operator==(rhs);
        }
    };

    struct StencilFace {
        constexpr StencilFace(Maxwell::StencilOp action_stencil_fail,
                              Maxwell::StencilOp action_depth_fail,
                              Maxwell::StencilOp action_depth_pass, Maxwell::ComparisonOp test_func)
            : action_stencil_fail{action_stencil_fail}, action_depth_fail{action_depth_fail},
              action_depth_pass{action_depth_pass}, test_func{test_func} {}
        StencilFace() = default;

        Maxwell::StencilOp action_stencil_fail;
        Maxwell::StencilOp action_depth_fail;
        Maxwell::StencilOp action_depth_pass;
        Maxwell::ComparisonOp test_func;

        std::size_t Hash() const noexcept;

        bool operator==(const StencilFace& rhs) const noexcept;

        bool operator!=(const StencilFace& rhs) const noexcept {
            return !operator==(rhs);
        }
    };

    struct BlendingAttachment {
        constexpr BlendingAttachment(bool enable, Maxwell::Blend::Equation rgb_equation,
                                     Maxwell::Blend::Factor src_rgb_func,
                                     Maxwell::Blend::Factor dst_rgb_func,
                                     Maxwell::Blend::Equation a_equation,
                                     Maxwell::Blend::Factor src_a_func,
                                     Maxwell::Blend::Factor dst_a_func,
                                     std::array<bool, 4> components)
            : enable{enable}, rgb_equation{rgb_equation}, src_rgb_func{src_rgb_func},
              dst_rgb_func{dst_rgb_func}, a_equation{a_equation}, src_a_func{src_a_func},
              dst_a_func{dst_a_func}, components{components} {}
        BlendingAttachment() = default;

        bool enable;
        Maxwell::Blend::Equation rgb_equation;
        Maxwell::Blend::Factor src_rgb_func;
        Maxwell::Blend::Factor dst_rgb_func;
        Maxwell::Blend::Equation a_equation;
        Maxwell::Blend::Factor src_a_func;
        Maxwell::Blend::Factor dst_a_func;
        std::array<bool, 4> components;

        std::size_t Hash() const noexcept;

        bool operator==(const BlendingAttachment& rhs) const noexcept;

        bool operator!=(const BlendingAttachment& rhs) const noexcept {
            return !operator==(rhs);
        }
    };

    struct VertexInput {
        std::size_t num_bindings = 0;
        std::size_t num_attributes = 0;
        std::array<VertexBinding, Maxwell::NumVertexArrays> bindings;
        std::array<VertexAttribute, Maxwell::NumVertexAttributes> attributes;

        std::size_t Hash() const noexcept;

        bool operator==(const VertexInput& rhs) const noexcept;

        bool operator!=(const VertexInput& rhs) const noexcept {
            return !operator==(rhs);
        }
    };

    struct InputAssembly {
        constexpr InputAssembly(Maxwell::PrimitiveTopology topology, bool primitive_restart_enable,
                                float point_size)
            : topology{topology}, primitive_restart_enable{primitive_restart_enable},
              point_size{point_size} {}
        InputAssembly() = default;

        Maxwell::PrimitiveTopology topology;
        bool primitive_restart_enable;
        float point_size;

        std::size_t Hash() const noexcept;

        bool operator==(const InputAssembly& rhs) const noexcept;

        bool operator!=(const InputAssembly& rhs) const noexcept {
            return !operator==(rhs);
        }
    };

    struct Tessellation {
        constexpr Tessellation(u32 patch_control_points, Maxwell::TessellationPrimitive primitive,
                               Maxwell::TessellationSpacing spacing, bool clockwise)
            : patch_control_points{patch_control_points}, primitive{primitive}, spacing{spacing},
              clockwise{clockwise} {}
        Tessellation() = default;

        u32 patch_control_points;
        Maxwell::TessellationPrimitive primitive;
        Maxwell::TessellationSpacing spacing;
        bool clockwise;

        std::size_t Hash() const noexcept;

        bool operator==(const Tessellation& rhs) const noexcept;

        bool operator!=(const Tessellation& rhs) const noexcept {
            return !operator==(rhs);
        }
    };

    struct Rasterizer {
        constexpr Rasterizer(bool cull_enable, bool depth_bias_enable, bool depth_clamp_enable,
                             bool ndc_minus_one_to_one, Maxwell::CullFace cull_face,
                             Maxwell::FrontFace front_face)
            : cull_enable{cull_enable}, depth_bias_enable{depth_bias_enable},
              depth_clamp_enable{depth_clamp_enable}, ndc_minus_one_to_one{ndc_minus_one_to_one},
              cull_face{cull_face}, front_face{front_face} {}
        Rasterizer() = default;

        bool cull_enable;
        bool depth_bias_enable;
        bool depth_clamp_enable;
        bool ndc_minus_one_to_one;
        Maxwell::CullFace cull_face;
        Maxwell::FrontFace front_face;

        std::size_t Hash() const noexcept;

        bool operator==(const Rasterizer& rhs) const noexcept;

        bool operator!=(const Rasterizer& rhs) const noexcept {
            return !operator==(rhs);
        }
    };

    struct DepthStencil {
        constexpr DepthStencil(bool depth_test_enable, bool depth_write_enable,
                               bool depth_bounds_enable, bool stencil_enable,
                               Maxwell::ComparisonOp depth_test_function, StencilFace front_stencil,
                               StencilFace back_stencil)
            : depth_test_enable{depth_test_enable}, depth_write_enable{depth_write_enable},
              depth_bounds_enable{depth_bounds_enable}, stencil_enable{stencil_enable},
              depth_test_function{depth_test_function}, front_stencil{front_stencil},
              back_stencil{back_stencil} {}
        DepthStencil() = default;

        bool depth_test_enable;
        bool depth_write_enable;
        bool depth_bounds_enable;
        bool stencil_enable;
        Maxwell::ComparisonOp depth_test_function;
        StencilFace front_stencil;
        StencilFace back_stencil;

        std::size_t Hash() const noexcept;

        bool operator==(const DepthStencil& rhs) const noexcept;

        bool operator!=(const DepthStencil& rhs) const noexcept {
            return !operator==(rhs);
        }
    };

    struct ColorBlending {
        constexpr ColorBlending(
            std::array<float, 4> blend_constants, std::size_t attachments_count,
            std::array<BlendingAttachment, Maxwell::NumRenderTargets> attachments)
            : attachments_count{attachments_count}, attachments{attachments} {}
        ColorBlending() = default;

        std::size_t attachments_count;
        std::array<BlendingAttachment, Maxwell::NumRenderTargets> attachments;

        std::size_t Hash() const noexcept;

        bool operator==(const ColorBlending& rhs) const noexcept;

        bool operator!=(const ColorBlending& rhs) const noexcept {
            return !operator==(rhs);
        }
    };

    std::size_t Hash() const noexcept;

    bool operator==(const FixedPipelineState& rhs) const noexcept;

    bool operator!=(const FixedPipelineState& rhs) const noexcept {
        return !operator==(rhs);
    }

    VertexInput vertex_input;
    InputAssembly input_assembly;
    Tessellation tessellation;
    Rasterizer rasterizer;
    DepthStencil depth_stencil;
    ColorBlending color_blending;
};
static_assert(std::is_trivially_copyable_v<FixedPipelineState::VertexBinding>);
static_assert(std::is_trivially_copyable_v<FixedPipelineState::VertexAttribute>);
static_assert(std::is_trivially_copyable_v<FixedPipelineState::StencilFace>);
static_assert(std::is_trivially_copyable_v<FixedPipelineState::BlendingAttachment>);
static_assert(std::is_trivially_copyable_v<FixedPipelineState::VertexInput>);
static_assert(std::is_trivially_copyable_v<FixedPipelineState::InputAssembly>);
static_assert(std::is_trivially_copyable_v<FixedPipelineState::Tessellation>);
static_assert(std::is_trivially_copyable_v<FixedPipelineState::Rasterizer>);
static_assert(std::is_trivially_copyable_v<FixedPipelineState::DepthStencil>);
static_assert(std::is_trivially_copyable_v<FixedPipelineState::ColorBlending>);
static_assert(std::is_trivially_copyable_v<FixedPipelineState>);

FixedPipelineState GetFixedPipelineState(const Maxwell& regs);

} // namespace Vulkan

namespace std {

template <>
struct hash<Vulkan::FixedPipelineState> {
    std::size_t operator()(const Vulkan::FixedPipelineState& k) const noexcept {
        return k.Hash();
    }
};

} // namespace std
