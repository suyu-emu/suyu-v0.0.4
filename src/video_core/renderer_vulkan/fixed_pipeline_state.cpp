// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include <tuple>

#include <boost/functional/hash.hpp>

#include "common/cityhash.h"
#include "common/common_types.h"
#include "video_core/renderer_vulkan/fixed_pipeline_state.h"

namespace Vulkan {

namespace {

constexpr std::size_t POINT = 0;
constexpr std::size_t LINE = 1;
constexpr std::size_t POLYGON = 2;
constexpr std::array POLYGON_OFFSET_ENABLE_LUT = {
    POINT,   // Points
    LINE,    // Lines
    LINE,    // LineLoop
    LINE,    // LineStrip
    POLYGON, // Triangles
    POLYGON, // TriangleStrip
    POLYGON, // TriangleFan
    POLYGON, // Quads
    POLYGON, // QuadStrip
    POLYGON, // Polygon
    LINE,    // LinesAdjacency
    LINE,    // LineStripAdjacency
    POLYGON, // TrianglesAdjacency
    POLYGON, // TriangleStripAdjacency
    POLYGON, // Patches
};

} // Anonymous namespace

void FixedPipelineState::DepthStencil::Fill(const Maxwell& regs) noexcept {
    raw = 0;
    front.action_stencil_fail.Assign(PackStencilOp(regs.stencil_front_op_fail));
    front.action_depth_fail.Assign(PackStencilOp(regs.stencil_front_op_zfail));
    front.action_depth_pass.Assign(PackStencilOp(regs.stencil_front_op_zpass));
    front.test_func.Assign(PackComparisonOp(regs.stencil_front_func_func));
    if (regs.stencil_two_side_enable) {
        back.action_stencil_fail.Assign(PackStencilOp(regs.stencil_back_op_fail));
        back.action_depth_fail.Assign(PackStencilOp(regs.stencil_back_op_zfail));
        back.action_depth_pass.Assign(PackStencilOp(regs.stencil_back_op_zpass));
        back.test_func.Assign(PackComparisonOp(regs.stencil_back_func_func));
    } else {
        back.action_stencil_fail.Assign(front.action_stencil_fail);
        back.action_depth_fail.Assign(front.action_depth_fail);
        back.action_depth_pass.Assign(front.action_depth_pass);
        back.test_func.Assign(front.test_func);
    }
    depth_test_enable.Assign(regs.depth_test_enable);
    depth_write_enable.Assign(regs.depth_write_enabled);
    depth_bounds_enable.Assign(regs.depth_bounds_enable);
    stencil_enable.Assign(regs.stencil_enable);
    depth_test_func.Assign(PackComparisonOp(regs.depth_test_func));
}

void FixedPipelineState::Rasterizer::Fill(const Maxwell& regs) noexcept {
    const auto& clip = regs.view_volume_clip_control;
    const std::array enabled_lut = {regs.polygon_offset_point_enable,
                                    regs.polygon_offset_line_enable,
                                    regs.polygon_offset_fill_enable};
    const u32 topology_index = static_cast<u32>(regs.draw.topology.Value());

    u32 packed_front_face = PackFrontFace(regs.front_face);
    if (regs.screen_y_control.triangle_rast_flip != 0 &&
        regs.viewport_transform[0].scale_y > 0.0f) {
        // Flip front face
        packed_front_face = 1 - packed_front_face;
    }

    raw = 0;
    topology.Assign(topology_index);
    primitive_restart_enable.Assign(regs.primitive_restart.enabled != 0 ? 1 : 0);
    cull_enable.Assign(regs.cull_test_enabled != 0 ? 1 : 0);
    depth_bias_enable.Assign(enabled_lut[POLYGON_OFFSET_ENABLE_LUT[topology_index]] != 0 ? 1 : 0);
    depth_clamp_enable.Assign(clip.depth_clamp_near == 1 || clip.depth_clamp_far == 1 ? 1 : 0);
    ndc_minus_one_to_one.Assign(regs.depth_mode == Maxwell::DepthMode::MinusOneToOne ? 1 : 0);
    cull_face.Assign(PackCullFace(regs.cull_face));
    front_face.Assign(packed_front_face);
    polygon_mode.Assign(PackPolygonMode(regs.polygon_mode_front));
    patch_control_points_minus_one.Assign(regs.patch_vertices - 1);
    tessellation_primitive.Assign(static_cast<u32>(regs.tess_mode.prim.Value()));
    tessellation_spacing.Assign(static_cast<u32>(regs.tess_mode.spacing.Value()));
    tessellation_clockwise.Assign(regs.tess_mode.cw.Value());
    logic_op_enable.Assign(regs.logic_op.enable != 0 ? 1 : 0);
    logic_op.Assign(PackLogicOp(regs.logic_op.operation));
    std::memcpy(&point_size, &regs.point_size, sizeof(point_size)); // TODO: C++20 std::bit_cast
}

namespace {

constexpr FixedPipelineState::BlendingAttachment GetBlendingAttachmentState(
    const Maxwell& regs, std::size_t render_target) {
    const auto& mask = regs.color_mask[regs.color_mask_common ? 0 : render_target];
    const std::array components = {mask.R != 0, mask.G != 0, mask.B != 0, mask.A != 0};

    const FixedPipelineState::BlendingAttachment default_blending(
        false, Maxwell::Blend::Equation::Add, Maxwell::Blend::Factor::One,
        Maxwell::Blend::Factor::Zero, Maxwell::Blend::Equation::Add, Maxwell::Blend::Factor::One,
        Maxwell::Blend::Factor::Zero, components);
    if (render_target >= regs.rt_control.count) {
        return default_blending;
    }

    if (!regs.independent_blend_enable) {
        const auto& src = regs.blend;
        if (!src.enable[render_target]) {
            return default_blending;
        }
        return FixedPipelineState::BlendingAttachment(
            true, src.equation_rgb, src.factor_source_rgb, src.factor_dest_rgb, src.equation_a,
            src.factor_source_a, src.factor_dest_a, components);
    }

    if (!regs.blend.enable[render_target]) {
        return default_blending;
    }
    const auto& src = regs.independent_blend[render_target];
    return FixedPipelineState::BlendingAttachment(
        true, src.equation_rgb, src.factor_source_rgb, src.factor_dest_rgb, src.equation_a,
        src.factor_source_a, src.factor_dest_a, components);
}

constexpr FixedPipelineState::ColorBlending GetColorBlendingState(const Maxwell& regs) {
    return FixedPipelineState::ColorBlending(
        {regs.blend_color.r, regs.blend_color.g, regs.blend_color.b, regs.blend_color.a},
        regs.rt_control.count,
        {GetBlendingAttachmentState(regs, 0), GetBlendingAttachmentState(regs, 1),
         GetBlendingAttachmentState(regs, 2), GetBlendingAttachmentState(regs, 3),
         GetBlendingAttachmentState(regs, 4), GetBlendingAttachmentState(regs, 5),
         GetBlendingAttachmentState(regs, 6), GetBlendingAttachmentState(regs, 7)});
}

} // Anonymous namespace

std::size_t FixedPipelineState::BlendingAttachment::Hash() const noexcept {
    return static_cast<std::size_t>(enable) ^ (static_cast<std::size_t>(rgb_equation) << 5) ^
           (static_cast<std::size_t>(src_rgb_func) << 10) ^
           (static_cast<std::size_t>(dst_rgb_func) << 15) ^
           (static_cast<std::size_t>(a_equation) << 20) ^
           (static_cast<std::size_t>(src_a_func) << 25) ^
           (static_cast<std::size_t>(dst_a_func) << 30) ^
           (static_cast<std::size_t>(components[0]) << 35) ^
           (static_cast<std::size_t>(components[1]) << 36) ^
           (static_cast<std::size_t>(components[2]) << 37) ^
           (static_cast<std::size_t>(components[3]) << 38);
}

bool FixedPipelineState::BlendingAttachment::operator==(const BlendingAttachment& rhs) const
    noexcept {
    return std::tie(enable, rgb_equation, src_rgb_func, dst_rgb_func, a_equation, src_a_func,
                    dst_a_func, components) ==
           std::tie(rhs.enable, rhs.rgb_equation, rhs.src_rgb_func, rhs.dst_rgb_func,
                    rhs.a_equation, rhs.src_a_func, rhs.dst_a_func, rhs.components);
}

std::size_t FixedPipelineState::VertexInput::Hash() const noexcept {
    // TODO(Rodrigo): Replace this
    return Common::CityHash64(reinterpret_cast<const char*>(this), sizeof *this);
}

bool FixedPipelineState::VertexInput::operator==(const VertexInput& rhs) const noexcept {
    return std::memcmp(this, &rhs, sizeof *this) == 0;
}

std::size_t FixedPipelineState::Rasterizer::Hash() const noexcept {
    u64 hash = static_cast<u64>(raw) << 32;
    std::memcpy(&hash, &point_size, sizeof(u32));
    return static_cast<std::size_t>(hash);
}

bool FixedPipelineState::Rasterizer::operator==(const Rasterizer& rhs) const noexcept {
    return raw == rhs.raw && point_size == rhs.point_size;
}

std::size_t FixedPipelineState::DepthStencil::Hash() const noexcept {
    return raw;
}

bool FixedPipelineState::DepthStencil::operator==(const DepthStencil& rhs) const noexcept {
    return raw == rhs.raw;
}

std::size_t FixedPipelineState::ColorBlending::Hash() const noexcept {
    std::size_t hash = attachments_count << 13;
    for (std::size_t rt = 0; rt < static_cast<std::size_t>(attachments_count); ++rt) {
        boost::hash_combine(hash, attachments[rt].Hash());
    }
    return hash;
}

bool FixedPipelineState::ColorBlending::operator==(const ColorBlending& rhs) const noexcept {
    return std::equal(attachments.begin(), attachments.begin() + attachments_count,
                      rhs.attachments.begin(), rhs.attachments.begin() + rhs.attachments_count);
}

std::size_t FixedPipelineState::Hash() const noexcept {
    std::size_t hash = 0;
    boost::hash_combine(hash, vertex_input.Hash());
    boost::hash_combine(hash, rasterizer.Hash());
    boost::hash_combine(hash, depth_stencil.Hash());
    boost::hash_combine(hash, color_blending.Hash());
    return hash;
}

bool FixedPipelineState::operator==(const FixedPipelineState& rhs) const noexcept {
    return std::tie(vertex_input, rasterizer, depth_stencil, color_blending) ==
           std::tie(rhs.vertex_input, rhs.rasterizer, rhs.depth_stencil, rhs.color_blending);
}

FixedPipelineState GetFixedPipelineState(const Maxwell& regs) {
    FixedPipelineState fixed_state;
    fixed_state.rasterizer.Fill(regs);
    fixed_state.depth_stencil.Fill(regs);
    fixed_state.color_blending = GetColorBlendingState(regs);
    return fixed_state;
}

u32 FixedPipelineState::PackComparisonOp(Maxwell::ComparisonOp op) noexcept {
    // OpenGL enums go from 0x200 to 0x207 and the others from 1 to 8
    // If we substract 0x200 to OpenGL enums and 1 to the others we get a 0-7 range.
    // Perfect for a hash.
    const u32 value = static_cast<u32>(op);
    return value - (value >= 0x200 ? 0x200 : 1);
}

Maxwell::ComparisonOp FixedPipelineState::UnpackComparisonOp(u32 packed) noexcept {
    // Read PackComparisonOp for the logic behind this.
    return static_cast<Maxwell::ComparisonOp>(packed + 1);
}

u32 FixedPipelineState::PackStencilOp(Maxwell::StencilOp op) noexcept {
    switch (op) {
    case Maxwell::StencilOp::Keep:
    case Maxwell::StencilOp::KeepOGL:
        return 0;
    case Maxwell::StencilOp::Zero:
    case Maxwell::StencilOp::ZeroOGL:
        return 1;
    case Maxwell::StencilOp::Replace:
    case Maxwell::StencilOp::ReplaceOGL:
        return 2;
    case Maxwell::StencilOp::Incr:
    case Maxwell::StencilOp::IncrOGL:
        return 3;
    case Maxwell::StencilOp::Decr:
    case Maxwell::StencilOp::DecrOGL:
        return 4;
    case Maxwell::StencilOp::Invert:
    case Maxwell::StencilOp::InvertOGL:
        return 5;
    case Maxwell::StencilOp::IncrWrap:
    case Maxwell::StencilOp::IncrWrapOGL:
        return 6;
    case Maxwell::StencilOp::DecrWrap:
    case Maxwell::StencilOp::DecrWrapOGL:
        return 7;
    }
    return 0;
}

Maxwell::StencilOp FixedPipelineState::UnpackStencilOp(u32 packed) noexcept {
    static constexpr std::array LUT = {Maxwell::StencilOp::Keep,     Maxwell::StencilOp::Zero,
                                       Maxwell::StencilOp::Replace,  Maxwell::StencilOp::Incr,
                                       Maxwell::StencilOp::Decr,     Maxwell::StencilOp::Invert,
                                       Maxwell::StencilOp::IncrWrap, Maxwell::StencilOp::DecrWrap};
    return LUT[packed];
}

u32 FixedPipelineState::PackCullFace(Maxwell::CullFace cull) noexcept {
    // FrontAndBack is 0x408, by substracting 0x406 in it we get 2.
    // Individual cull faces are in 0x404 and 0x405, substracting 0x404 we get 0 and 1.
    const u32 value = static_cast<u32>(cull);
    return value - (value == 0x408 ? 0x406 : 0x404);
}

Maxwell::CullFace FixedPipelineState::UnpackCullFace(u32 packed) noexcept {
    static constexpr std::array LUT = {Maxwell::CullFace::Front, Maxwell::CullFace::Back,
                                       Maxwell::CullFace::FrontAndBack};
    return LUT[packed];
}

u32 FixedPipelineState::PackFrontFace(Maxwell::FrontFace face) noexcept {
    return static_cast<u32>(face) - 0x900;
}

Maxwell::FrontFace FixedPipelineState::UnpackFrontFace(u32 packed) noexcept {
    return static_cast<Maxwell::FrontFace>(packed + 0x900);
}

u32 FixedPipelineState::PackPolygonMode(Maxwell::PolygonMode mode) noexcept {
    return static_cast<u32>(mode) - 0x1B00;
}

Maxwell::PolygonMode FixedPipelineState::UnpackPolygonMode(u32 packed) noexcept {
    return static_cast<Maxwell::PolygonMode>(packed + 0x1B00);
}

u32 FixedPipelineState::PackLogicOp(Maxwell::LogicOperation op) noexcept {
    return static_cast<u32>(op) - 0x1500;
}

Maxwell::LogicOperation FixedPipelineState::UnpackLogicOp(u32 packed) noexcept {
    return static_cast<Maxwell::LogicOperation>(packed + 0x1500);
}

} // namespace Vulkan
