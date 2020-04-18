// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <type_traits>

#include "common/bit_field.h"
#include "common/common_types.h"

#include "video_core/engines/maxwell_3d.h"
#include "video_core/surface.h"

namespace Vulkan {

using Maxwell = Tegra::Engines::Maxwell3D::Regs;

// TODO(Rodrigo): Optimize this structure.

template <class T>
inline constexpr bool IsHashable = std::has_unique_object_representations_v<T>&&
    std::is_trivially_copyable_v<T>&& std::is_trivially_constructible_v<T>;

struct FixedPipelineState {
    static u32 PackComparisonOp(Maxwell::ComparisonOp op) noexcept;
    static Maxwell::ComparisonOp UnpackComparisonOp(u32 packed) noexcept;

    static u32 PackStencilOp(Maxwell::StencilOp op) noexcept;
    static Maxwell::StencilOp UnpackStencilOp(u32 packed) noexcept;

    static u32 PackCullFace(Maxwell::CullFace cull) noexcept;
    static Maxwell::CullFace UnpackCullFace(u32 packed) noexcept;

    static u32 PackFrontFace(Maxwell::FrontFace face) noexcept;
    static Maxwell::FrontFace UnpackFrontFace(u32 packed) noexcept;

    static u32 PackPolygonMode(Maxwell::PolygonMode mode) noexcept;
    static Maxwell::PolygonMode UnpackPolygonMode(u32 packed) noexcept;

    static u32 PackLogicOp(Maxwell::LogicOperation op) noexcept;
    static Maxwell::LogicOperation UnpackLogicOp(u32 packed) noexcept;

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
        union Binding {
            u16 raw;
            BitField<0, 1, u16> enabled;
            BitField<1, 12, u16> stride;
        };

        union Attribute {
            u32 raw;
            BitField<0, 1, u32> enabled;
            BitField<1, 5, u32> buffer;
            BitField<6, 14, u32> offset;
            BitField<20, 3, u32> type;
            BitField<23, 6, u32> size;

            constexpr Maxwell::VertexAttribute::Type Type() const noexcept {
                return static_cast<Maxwell::VertexAttribute::Type>(type.Value());
            }

            constexpr Maxwell::VertexAttribute::Size Size() const noexcept {
                return static_cast<Maxwell::VertexAttribute::Size>(size.Value());
            }
        };

        std::array<Binding, Maxwell::NumVertexArrays> bindings;
        std::array<u32, Maxwell::NumVertexArrays> binding_divisors;
        std::array<Attribute, Maxwell::NumVertexAttributes> attributes;

        void SetBinding(std::size_t index, bool enabled, u32 stride, u32 divisor) noexcept {
            auto& binding = bindings[index];
            binding.raw = 0;
            binding.enabled.Assign(enabled ? 1 : 0);
            binding.stride.Assign(stride);
            binding_divisors[index] = divisor;
        }

        void SetAttribute(std::size_t index, bool enabled, u32 buffer, u32 offset,
                          Maxwell::VertexAttribute::Type type,
                          Maxwell::VertexAttribute::Size size) noexcept {
            auto& attribute = attributes[index];
            attribute.raw = 0;
            attribute.enabled.Assign(enabled ? 1 : 0);
            attribute.buffer.Assign(buffer);
            attribute.offset.Assign(offset);
            attribute.type.Assign(static_cast<u32>(type));
            attribute.size.Assign(static_cast<u32>(size));
        }

        std::size_t Hash() const noexcept;

        bool operator==(const VertexInput& rhs) const noexcept;

        bool operator!=(const VertexInput& rhs) const noexcept {
            return !operator==(rhs);
        }
    };
    static_assert(IsHashable<VertexInput>);

    struct Rasterizer {
        union {
            u32 raw;
            BitField<0, 4, u32> topology;
            BitField<4, 1, u32> primitive_restart_enable;
            BitField<5, 1, u32> cull_enable;
            BitField<6, 1, u32> depth_bias_enable;
            BitField<7, 1, u32> depth_clamp_enable;
            BitField<8, 1, u32> ndc_minus_one_to_one;
            BitField<9, 2, u32> cull_face;
            BitField<11, 1, u32> front_face;
            BitField<12, 2, u32> polygon_mode;
            BitField<14, 5, u32> patch_control_points_minus_one;
            BitField<19, 2, u32> tessellation_primitive;
            BitField<21, 2, u32> tessellation_spacing;
            BitField<23, 1, u32> tessellation_clockwise;
            BitField<24, 1, u32> logic_op_enable;
            BitField<25, 4, u32> logic_op;
        };

        // TODO(Rodrigo): Move this to push constants
        u32 point_size;

        void Fill(const Maxwell& regs) noexcept;

        std::size_t Hash() const noexcept;

        bool operator==(const Rasterizer& rhs) const noexcept;

        bool operator!=(const Rasterizer& rhs) const noexcept {
            return !operator==(rhs);
        }

        constexpr Maxwell::PrimitiveTopology Topology() const noexcept {
            return static_cast<Maxwell::PrimitiveTopology>(topology.Value());
        }

        Maxwell::CullFace CullFace() const noexcept {
            return UnpackCullFace(cull_face.Value());
        }

        Maxwell::FrontFace FrontFace() const noexcept {
            return UnpackFrontFace(front_face.Value());
        }
    };
    static_assert(IsHashable<Rasterizer>);

    struct DepthStencil {
        template <std::size_t Position>
        union StencilFace {
            BitField<Position + 0, 3, u32> action_stencil_fail;
            BitField<Position + 3, 3, u32> action_depth_fail;
            BitField<Position + 6, 3, u32> action_depth_pass;
            BitField<Position + 9, 3, u32> test_func;

            Maxwell::StencilOp ActionStencilFail() const noexcept {
                return UnpackStencilOp(action_stencil_fail);
            }

            Maxwell::StencilOp ActionDepthFail() const noexcept {
                return UnpackStencilOp(action_depth_fail);
            }

            Maxwell::StencilOp ActionDepthPass() const noexcept {
                return UnpackStencilOp(action_depth_pass);
            }

            Maxwell::ComparisonOp TestFunc() const noexcept {
                return UnpackComparisonOp(test_func);
            }
        };

        union {
            u32 raw;
            StencilFace<0> front;
            StencilFace<12> back;
            BitField<24, 1, u32> depth_test_enable;
            BitField<25, 1, u32> depth_write_enable;
            BitField<26, 1, u32> depth_bounds_enable;
            BitField<27, 1, u32> stencil_enable;
            BitField<28, 3, u32> depth_test_func;
        };

        void Fill(const Maxwell& regs) noexcept;

        std::size_t Hash() const noexcept;

        bool operator==(const DepthStencil& rhs) const noexcept;

        bool operator!=(const DepthStencil& rhs) const noexcept {
            return !operator==(rhs);
        }

        Maxwell::ComparisonOp DepthTestFunc() const noexcept {
            return UnpackComparisonOp(depth_test_func);
        }
    };
    static_assert(IsHashable<DepthStencil>);

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

    VertexInput vertex_input;
    Rasterizer rasterizer;
    DepthStencil depth_stencil;
    ColorBlending color_blending;

    std::size_t Hash() const noexcept;

    bool operator==(const FixedPipelineState& rhs) const noexcept;

    bool operator!=(const FixedPipelineState& rhs) const noexcept {
        return !operator==(rhs);
    }
};
static_assert(std::is_trivially_copyable_v<FixedPipelineState::BlendingAttachment>);
static_assert(std::is_trivially_copyable_v<FixedPipelineState::VertexInput>);
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
