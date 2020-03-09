// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <tuple>

#include <boost/functional/hash.hpp>

#include "common/common_types.h"
#include "video_core/renderer_vulkan/fixed_pipeline_state.h"

namespace Vulkan {

namespace {

constexpr FixedPipelineState::DepthStencil GetDepthStencilState(const Maxwell& regs) {
    const FixedPipelineState::StencilFace front_stencil(
        regs.stencil_front_op_fail, regs.stencil_front_op_zfail, regs.stencil_front_op_zpass,
        regs.stencil_front_func_func);
    const FixedPipelineState::StencilFace back_stencil =
        regs.stencil_two_side_enable
            ? FixedPipelineState::StencilFace(regs.stencil_back_op_fail, regs.stencil_back_op_zfail,
                                              regs.stencil_back_op_zpass,
                                              regs.stencil_back_func_func)
            : front_stencil;
    return FixedPipelineState::DepthStencil(
        regs.depth_test_enable == 1, regs.depth_write_enabled == 1, regs.depth_bounds_enable == 1,
        regs.stencil_enable == 1, regs.depth_test_func, front_stencil, back_stencil);
}

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

std::size_t FixedPipelineState::VertexBinding::Hash() const noexcept {
    return (index << stride) ^ divisor;
}

bool FixedPipelineState::VertexBinding::operator==(const VertexBinding& rhs) const noexcept {
    return std::tie(index, stride, divisor) == std::tie(rhs.index, rhs.stride, rhs.divisor);
}

std::size_t FixedPipelineState::VertexAttribute::Hash() const noexcept {
    return static_cast<std::size_t>(index) ^ (static_cast<std::size_t>(buffer) << 13) ^
           (static_cast<std::size_t>(type) << 22) ^ (static_cast<std::size_t>(size) << 31) ^
           (static_cast<std::size_t>(offset) << 36);
}

bool FixedPipelineState::VertexAttribute::operator==(const VertexAttribute& rhs) const noexcept {
    return std::tie(index, buffer, type, size, offset) ==
           std::tie(rhs.index, rhs.buffer, rhs.type, rhs.size, rhs.offset);
}

std::size_t FixedPipelineState::StencilFace::Hash() const noexcept {
    return static_cast<std::size_t>(action_stencil_fail) ^
           (static_cast<std::size_t>(action_depth_fail) << 4) ^
           (static_cast<std::size_t>(action_depth_fail) << 20) ^
           (static_cast<std::size_t>(action_depth_pass) << 36);
}

bool FixedPipelineState::StencilFace::operator==(const StencilFace& rhs) const noexcept {
    return std::tie(action_stencil_fail, action_depth_fail, action_depth_pass, test_func) ==
           std::tie(rhs.action_stencil_fail, rhs.action_depth_fail, rhs.action_depth_pass,
                    rhs.test_func);
}

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
    std::size_t hash = num_bindings ^ (num_attributes << 32);
    for (std::size_t i = 0; i < num_bindings; ++i) {
        boost::hash_combine(hash, bindings[i].Hash());
    }
    for (std::size_t i = 0; i < num_attributes; ++i) {
        boost::hash_combine(hash, attributes[i].Hash());
    }
    return hash;
}

bool FixedPipelineState::VertexInput::operator==(const VertexInput& rhs) const noexcept {
    return std::equal(bindings.begin(), bindings.begin() + num_bindings, rhs.bindings.begin(),
                      rhs.bindings.begin() + rhs.num_bindings) &&
           std::equal(attributes.begin(), attributes.begin() + num_attributes,
                      rhs.attributes.begin(), rhs.attributes.begin() + rhs.num_attributes);
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
    std::size_t hash = static_cast<std::size_t>(depth_test_enable) ^
                       (static_cast<std::size_t>(depth_write_enable) << 1) ^
                       (static_cast<std::size_t>(depth_bounds_enable) << 2) ^
                       (static_cast<std::size_t>(stencil_enable) << 3) ^
                       (static_cast<std::size_t>(depth_test_function) << 4);
    boost::hash_combine(hash, front_stencil.Hash());
    boost::hash_combine(hash, back_stencil.Hash());
    return hash;
}

bool FixedPipelineState::DepthStencil::operator==(const DepthStencil& rhs) const noexcept {
    return std::tie(depth_test_enable, depth_write_enable, depth_bounds_enable, depth_test_function,
                    stencil_enable, front_stencil, back_stencil) ==
           std::tie(rhs.depth_test_enable, rhs.depth_write_enable, rhs.depth_bounds_enable,
                    rhs.depth_test_function, rhs.stencil_enable, rhs.front_stencil,
                    rhs.back_stencil);
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
    fixed_state.depth_stencil = GetDepthStencilState(regs);
    fixed_state.color_blending = GetColorBlendingState(regs);
    return fixed_state;
}

} // namespace Vulkan
