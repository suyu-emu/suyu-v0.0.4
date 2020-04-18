// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <tuple>

#include <boost/functional/hash.hpp>

#include "common/cityhash.h"
#include "common/common_types.h"
#include "video_core/renderer_vulkan/fixed_pipeline_state.h"

namespace Vulkan {

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

namespace {

constexpr FixedPipelineState::InputAssembly GetInputAssemblyState(const Maxwell& regs) {
    return FixedPipelineState::InputAssembly(
        regs.draw.topology, regs.primitive_restart.enabled,
        regs.draw.topology == Maxwell::PrimitiveTopology::Points ? regs.point_size : 0.0f);
}

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

constexpr FixedPipelineState::Tessellation GetTessellationState(const Maxwell& regs) {
    return FixedPipelineState::Tessellation(regs.patch_vertices, regs.tess_mode.prim,
                                            regs.tess_mode.spacing, regs.tess_mode.cw != 0);
}

constexpr std::size_t Point = 0;
constexpr std::size_t Line = 1;
constexpr std::size_t Polygon = 2;
constexpr std::array PolygonOffsetEnableLUT = {
    Point,   // Points
    Line,    // Lines
    Line,    // LineLoop
    Line,    // LineStrip
    Polygon, // Triangles
    Polygon, // TriangleStrip
    Polygon, // TriangleFan
    Polygon, // Quads
    Polygon, // QuadStrip
    Polygon, // Polygon
    Line,    // LinesAdjacency
    Line,    // LineStripAdjacency
    Polygon, // TrianglesAdjacency
    Polygon, // TriangleStripAdjacency
    Polygon, // Patches
};

constexpr FixedPipelineState::Rasterizer GetRasterizerState(const Maxwell& regs) {
    const std::array enabled_lut = {regs.polygon_offset_point_enable,
                                    regs.polygon_offset_line_enable,
                                    regs.polygon_offset_fill_enable};
    const auto topology = static_cast<std::size_t>(regs.draw.topology.Value());
    const bool depth_bias_enabled = enabled_lut[PolygonOffsetEnableLUT[topology]];

    const auto& clip = regs.view_volume_clip_control;
    const bool depth_clamp_enabled = clip.depth_clamp_near == 1 || clip.depth_clamp_far == 1;

    Maxwell::FrontFace front_face = regs.front_face;
    if (regs.screen_y_control.triangle_rast_flip != 0 &&
        regs.viewport_transform[0].scale_y > 0.0f) {
        if (front_face == Maxwell::FrontFace::CounterClockWise)
            front_face = Maxwell::FrontFace::ClockWise;
        else if (front_face == Maxwell::FrontFace::ClockWise)
            front_face = Maxwell::FrontFace::CounterClockWise;
    }

    const bool gl_ndc = regs.depth_mode == Maxwell::DepthMode::MinusOneToOne;
    return FixedPipelineState::Rasterizer(regs.cull_test_enabled, depth_bias_enabled,
                                          depth_clamp_enabled, gl_ndc, regs.cull_face, front_face);
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

std::size_t FixedPipelineState::InputAssembly::Hash() const noexcept {
    std::size_t point_size_int = 0;
    std::memcpy(&point_size_int, &point_size, sizeof(point_size));
    return (static_cast<std::size_t>(topology) << 24) ^ (point_size_int << 32) ^
           static_cast<std::size_t>(primitive_restart_enable);
}

bool FixedPipelineState::InputAssembly::operator==(const InputAssembly& rhs) const noexcept {
    return std::tie(topology, primitive_restart_enable, point_size) ==
           std::tie(rhs.topology, rhs.primitive_restart_enable, rhs.point_size);
}

std::size_t FixedPipelineState::Tessellation::Hash() const noexcept {
    return static_cast<std::size_t>(patch_control_points) ^
           (static_cast<std::size_t>(primitive) << 6) ^ (static_cast<std::size_t>(spacing) << 8) ^
           (static_cast<std::size_t>(clockwise) << 10);
}

bool FixedPipelineState::Tessellation::operator==(const Tessellation& rhs) const noexcept {
    return std::tie(patch_control_points, primitive, spacing, clockwise) ==
           std::tie(rhs.patch_control_points, rhs.primitive, rhs.spacing, rhs.clockwise);
}

std::size_t FixedPipelineState::Rasterizer::Hash() const noexcept {
    return static_cast<std::size_t>(cull_enable) ^
           (static_cast<std::size_t>(depth_bias_enable) << 1) ^
           (static_cast<std::size_t>(depth_clamp_enable) << 2) ^
           (static_cast<std::size_t>(ndc_minus_one_to_one) << 3) ^
           (static_cast<std::size_t>(cull_face) << 24) ^
           (static_cast<std::size_t>(front_face) << 48);
}

bool FixedPipelineState::Rasterizer::operator==(const Rasterizer& rhs) const noexcept {
    return std::tie(cull_enable, depth_bias_enable, depth_clamp_enable, ndc_minus_one_to_one,
                    cull_face, front_face) ==
           std::tie(rhs.cull_enable, rhs.depth_bias_enable, rhs.depth_clamp_enable,
                    rhs.ndc_minus_one_to_one, rhs.cull_face, rhs.front_face);
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
    boost::hash_combine(hash, input_assembly.Hash());
    boost::hash_combine(hash, tessellation.Hash());
    boost::hash_combine(hash, rasterizer.Hash());
    boost::hash_combine(hash, depth_stencil.Hash());
    boost::hash_combine(hash, color_blending.Hash());
    return hash;
}

bool FixedPipelineState::operator==(const FixedPipelineState& rhs) const noexcept {
    return std::tie(vertex_input, input_assembly, tessellation, rasterizer, depth_stencil,
                    color_blending) == std::tie(rhs.vertex_input, rhs.input_assembly,
                                                rhs.tessellation, rhs.rasterizer, rhs.depth_stencil,
                                                rhs.color_blending);
}

FixedPipelineState GetFixedPipelineState(const Maxwell& regs) {
    FixedPipelineState fixed_state;
    fixed_state.input_assembly = GetInputAssemblyState(regs);
    fixed_state.tessellation = GetTessellationState(regs);
    fixed_state.rasterizer = GetRasterizerState(regs);
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

} // namespace Vulkan
