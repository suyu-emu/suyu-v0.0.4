// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/div_ceil.h"
#include "shader_recompiler/backend/bindings.h"
#include "shader_recompiler/backend/msl/msl_emit_context.h"
#include "shader_recompiler/frontend/ir/program.h"
#include "shader_recompiler/profile.h"
#include "shader_recompiler/runtime_info.h"

namespace Shader::Backend::MSL {
namespace {
u32 CbufIndex(size_t offset) {
    return (offset / 4) % 4;
}

char Swizzle(size_t offset) {
    return "xyzw"[CbufIndex(offset)];
}

std::string_view InterpDecorator(Interpolation interp) {
    switch (interp) {
    case Interpolation::Smooth:
        return "";
    case Interpolation::Flat:
        return "flat ";
    case Interpolation::NoPerspective:
        return "noperspective ";
    }
    throw InvalidArgument("Invalid interpolation {}", interp);
}

// TODO
std::string_view InputArrayDecorator(Stage stage) {
    switch (stage) {
    case Stage::Geometry:
    case Stage::TessellationControl:
    case Stage::TessellationEval:
        return "[]";
    default:
        return "";
    }
}

// TODO
std::string OutputDecorator(Stage stage, u32 size) {
    switch (stage) {
    case Stage::TessellationControl:
        return fmt::format("[{}]", size);
    default:
        return "";
    }
}

/*
// TODO
std::string_view GetTessMode(TessPrimitive primitive) {
    switch (primitive) {
    case TessPrimitive::Triangles:
        return "triangles";
    case TessPrimitive::Quads:
        return "quads";
    case TessPrimitive::Isolines:
        return "isolines";
    }
    throw InvalidArgument("Invalid tessellation primitive {}", primitive);
}

// TODO
std::string_view GetTessSpacing(TessSpacing spacing) {
    switch (spacing) {
    case TessSpacing::Equal:
        return "equal_spacing";
    case TessSpacing::FractionalOdd:
        return "fractional_odd_spacing";
    case TessSpacing::FractionalEven:
        return "fractional_even_spacing";
    }
    throw InvalidArgument("Invalid tessellation spacing {}", spacing);
}

// TODO
std::string_view InputPrimitive(InputTopology topology) {
    switch (topology) {
    case InputTopology::Points:
        return "points";
    case InputTopology::Lines:
        return "lines";
    case InputTopology::LinesAdjacency:
        return "lines_adjacency";
    case InputTopology::Triangles:
        return "triangles";
    case InputTopology::TrianglesAdjacency:
        return "triangles_adjacency";
    }
    throw InvalidArgument("Invalid input topology {}", topology);
}

// TODO
std::string_view OutputPrimitive(OutputTopology topology) {
    switch (topology) {
    case OutputTopology::PointList:
        return "points";
    case OutputTopology::LineStrip:
        return "line_strip";
    case OutputTopology::TriangleStrip:
        return "triangle_strip";
    }
    throw InvalidArgument("Invalid output topology {}", topology);
}
*/

// TODO
std::string_view DepthSamplerType(TextureType type) {
    switch (type) {
    case TextureType::Color1D:
        return "sampler1DShadow";
    case TextureType::ColorArray1D:
        return "sampler1DArrayShadow";
    case TextureType::Color2D:
        return "sampler2DShadow";
    case TextureType::ColorArray2D:
        return "sampler2DArrayShadow";
    case TextureType::ColorCube:
        return "samplerCubeShadow";
    case TextureType::ColorArrayCube:
        return "samplerCubeArrayShadow";
    default:
        throw NotImplementedException("Texture type: {}", type);
    }
}

// TODO: emit sampler as well
// TODO: handle multisample
// TODO: handle texture buffer
std::string_view ColorSamplerType(TextureType type, bool is_multisample = false) {
    if (is_multisample) {
        ASSERT(type == TextureType::Color2D || type == TextureType::ColorArray2D);
    }
    switch (type) {
    case TextureType::Color1D:
        return "texture1d";
    case TextureType::ColorArray1D:
        return "texture1d_array";
    case TextureType::Color2D:
    case TextureType::Color2DRect:
        return "texture2d";
    case TextureType::ColorArray2D:
        return "texture2d_array";
    case TextureType::Color3D:
        return "texture3d";
    case TextureType::ColorCube:
        return "texturecube";
    case TextureType::ColorArrayCube:
        return "texturecube_array";
    default:
        throw NotImplementedException("Texture type: {}", type);
    }
}

// TODO: handle texture buffer
std::string_view ImageType(TextureType type) {
    switch (type) {
    case TextureType::Color1D:
        return "texture1d";
    case TextureType::ColorArray1D:
        return "texture1d_array";
    case TextureType::Color2D:
        return "texture2d";
    case TextureType::ColorArray2D:
        return "texture2d_array";
    case TextureType::Color3D:
        return "texture3d";
    case TextureType::ColorCube:
        return "texturecube";
    case TextureType::ColorArrayCube:
        return "texturecube_array";
    default:
        throw NotImplementedException("Image type: {}", type);
    }
}

// TODO: is this needed?
/*
std::string_view ImageFormatString(ImageFormat format) {
    switch (format) {
    case ImageFormat::Typeless:
        return "";
    case ImageFormat::R8_UINT:
        return ",r8ui";
    case ImageFormat::R8_SINT:
        return ",r8i";
    case ImageFormat::R16_UINT:
        return ",r16ui";
    case ImageFormat::R16_SINT:
        return ",r16i";
    case ImageFormat::R32_UINT:
        return ",r32ui";
    case ImageFormat::R32G32_UINT:
        return ",rg32ui";
    case ImageFormat::R32G32B32A32_UINT:
        return ",rgba32ui";
    default:
        throw NotImplementedException("Image format: {}", format);
    }
}
*/

std::string_view ImageAccessQualifier(bool is_written, bool is_read) {
    if (is_written && is_read) {
        return "access::read, access::write";
    }
    if (is_written) {
        return "access::write";
    }
    if (is_read) {
        return "access::read";
    }
    return "";
}
} // Anonymous namespace

EmitContext::EmitContext(IR::Program& program, Bindings& bindings, const Profile& profile_,
                         const RuntimeInfo& runtime_info_)
    : info{program.info}, profile{profile_}, runtime_info{runtime_info_}, stage{program.stage},
      uses_geometry_passthrough{program.is_geometry_passthrough &&
                                profile.support_geometry_shader_passthrough} {
    if (profile.need_fastmath_off) {
        // TODO
    }
    switch (program.stage) {
    case Stage::VertexA:
    case Stage::VertexB:
        stage_name = "vertex";
        break;
    case Stage::TessellationControl:
        stage_name = "kernel";
        break;
    case Stage::TessellationEval:
        stage_name = "vertex";
        break;
    case Stage::Geometry:
        stage_name = "vertex";
        break;
    case Stage::Fragment:
        stage_name = "fragment";
        break;
    case Stage::Compute:
        stage_name = "kernel";
        const u32 local_x{std::max(program.workgroup_size[0], 1u)};
        const u32 local_y{std::max(program.workgroup_size[1], 1u)};
        const u32 local_z{std::max(program.workgroup_size[2], 1u)};
        header += fmt::format("layout(local_size_x={},local_size_y={},local_size_z={}) in;",
                              local_x, local_y, local_z);
        break;
    }
    // TODO
    // SetupOutPerVertex(*this, header);
    // SetupInPerVertex(*this, header);

    for (size_t index = 0; index < IR::NUM_GENERICS; ++index) {
        if (!info.loads.Generic(index) || !runtime_info.previous_stage_stores.Generic(index)) {
            continue;
        }
        const auto qualifier{uses_geometry_passthrough ? "passthrough"
                                                       : fmt::format("location={}", index)};
        header += fmt::format("layout({}){}in vec4 in_attr{}{};", qualifier,
                              InterpDecorator(info.interpolation[index]), index,
                              InputArrayDecorator(stage));
    }
    for (size_t index = 0; index < info.uses_patches.size(); ++index) {
        if (!info.uses_patches[index]) {
            continue;
        }
        const auto qualifier{stage == Stage::TessellationControl ? "out" : "in"};
        header += fmt::format("layout(location={})patch {} vec4 patch{};", index, qualifier, index);
    }
    if (stage == Stage::Fragment) {
        for (size_t index = 0; index < info.stores_frag_color.size(); ++index) {
            if (!info.stores_frag_color[index] && !profile.need_declared_frag_colors) {
                continue;
            }
            header += fmt::format("layout(location={})out vec4 frag_color{};", index, index);
        }
    }
    header += "struct __Output {\n";
    if (stage == Stage::VertexB || stage == Stage::Geometry) {
        header += "float4 position [[position]];\n";
    }
    for (size_t index = 0; index < IR::NUM_GENERICS; ++index) {
        if (info.stores.Generic(index)) {
            DefineGenericOutput(index, program.invocations);
        }
    }
    header += "};\n";
    bool added = DefineInputs(bindings);
    if (info.uses_rescaling_uniform) {
        if (added)
            input_str += ",";
        input_str += "constant float4& scaling";
        added = true;
    }
    if (info.uses_render_area) {
        if (added)
            input_str += ",";
        input_str += "constant float4& render_area";
        added = true;
    }
    DefineHelperFunctions();
    DefineConstants();
}

bool EmitContext::DefineInputs(Bindings& bindings) {
    bool added{false};

    // Constant buffers
    for (const auto& desc : info.constant_buffer_descriptors) {
        const u32 cbuf_used_size{Common::DivCeil(info.constant_buffer_used_sizes[desc.index], 16U)};
        const u32 cbuf_binding_size{info.uses_global_memory ? 0x1000U : cbuf_used_size};
        if (added)
            input_str += ",";
        input_str += fmt::format("constant float4& {}_cbuf{}[{}] [[buffer({})]]", stage_name,
                                 desc.index, cbuf_binding_size, bindings.uniform_buffer);
        bindings.uniform_buffer += desc.count;
        added = true;
    }

    // Constant buffer indirect
    // TODO

    // Storage space buffers
    u32 index{};
    for (const auto& desc : info.storage_buffers_descriptors) {
        if (added)
            input_str += ",";
        input_str += fmt::format("device uint& {}_ssbo{}[] [[buffer({})]]", stage_name, index,
                                 bindings.storage_buffer);
        bindings.storage_buffer += desc.count;
        index += desc.count;
        added = true;
    }

    // Images
    // TODO
    /*
    image_buffers.reserve(info.image_buffer_descriptors.size());
    for (const auto& desc : info.image_buffer_descriptors) {
        image_buffers.push_back({bindings.image, desc.count});
        const auto format{ImageFormatString(desc.format)};
        const auto qualifier{ImageAccessQualifier(desc.is_written, desc.is_read)};
        const auto array_decorator{desc.count > 1 ? fmt::format("[{}]", desc.count) : ""};
        input_str += fmt::format("layout(binding={}{}) uniform {}uimageBuffer img{}{};",
                              bindings.image, format, qualifier, bindings.image, array_decorator);
        bindings.image += desc.count;
    }
    */
    images.reserve(info.image_descriptors.size());
    for (const auto& desc : info.image_descriptors) {
        images.push_back({bindings.image, desc.count});
        // TODO: do we need format?
        // const auto format{ImageFormatString(desc.format)};
        const auto image_type{ImageType(desc.type)};
        const auto qualifier{ImageAccessQualifier(desc.is_written, desc.is_read)};
        const auto array_decorator{desc.count > 1 ? fmt::format("[{}]", desc.count) : ""};
        if (added)
            input_str += ",";
        input_str += fmt::format("{}<{}> {}_img{}{} [[texture({})]]", qualifier, image_type,
                                 stage_name, bindings.image, array_decorator, bindings.image);
        bindings.image += desc.count;
        added = true;
    }

    // Textures
    // TODO
    /*
    texture_buffers.reserve(info.texture_buffer_descriptors.size());
    for (const auto& desc : info.texture_buffer_descriptors) {
        texture_buffers.push_back({bindings.texture, desc.count});
        const auto sampler_type{ColorSamplerType(TextureType::Buffer)};
        const auto array_decorator{desc.count > 1 ? fmt::format("[{}]", desc.count) : ""};
        input_str += fmt::format("layout(binding={}) uniform {} tex{}{};", bindings.texture,
                              sampler_type, bindings.texture, array_decorator);
        bindings.texture += desc.count;
    }
    */
    textures.reserve(info.texture_descriptors.size());
    for (const auto& desc : info.texture_descriptors) {
        textures.push_back({bindings.texture, desc.count});
        const auto texture_type{desc.is_depth ? DepthSamplerType(desc.type)
                                              : ColorSamplerType(desc.type, desc.is_multisample)};
        const auto array_decorator{desc.count > 1 ? fmt::format("[{}]", desc.count) : ""};
        if (added)
            input_str += ",";
        input_str += fmt::format("{} {}_tex{}{} [[texture({})]]", texture_type, stage_name,
                                 bindings.texture, array_decorator, bindings.texture);
        input_str += fmt::format(",sampler {}_samp{}{} [[sampler({})]]", stage_name,
                                 bindings.texture, array_decorator, bindings.texture);
        bindings.texture += desc.count;
        added = true;
    }

    return added;
}

// TODO
void EmitContext::DefineGenericOutput(size_t index, u32 invocations) {
    const auto type{fmt::format("float{}", 4)};
    std::string name{fmt::format("attr{}", index)};
    header += fmt::format("{} {}{} [[user(locn{})]];\n", type, name,
                          OutputDecorator(stage, invocations), index);

    const GenericElementInfo element_info{
        .name = "__out." + name,
        .first_element = 0,
        .num_components = 4,
    };
    std::fill_n(output_generics[index].begin(), 4, element_info);
}

void EmitContext::DefineHelperFunctions() {
    if (info.uses_global_increment || info.uses_shared_increment) {
        header += "uint CasIncrement(uint op_a,uint op_b){return op_a>=op_b?0u:(op_a+1u);}";
    }
    if (info.uses_global_decrement || info.uses_shared_decrement) {
        header += "uint CasDecrement(uint op_a,uint op_b){"
                  "return op_a==0||op_a>op_b?op_b:(op_a-1u);}";
    }
    if (info.uses_atomic_f32_add) {
        header += "uint CasFloatAdd(uint op_a,float op_b){"
                  "return as_type<uint>(as_type<float>(op_a)+op_b);}";
    }
    if (info.uses_atomic_f32x2_add) {
        header += "uint CasFloatAdd32x2(uint op_a,vec2 op_b){"
                  "return packHalf2x16(unpackHalf2x16(op_a)+op_b);}";
    }
    if (info.uses_atomic_f32x2_min) {
        header += "uint CasFloatMin32x2(uint op_a,vec2 op_b){return "
                  "packHalf2x16(min(unpackHalf2x16(op_a),op_b));}";
    }
    if (info.uses_atomic_f32x2_max) {
        header += "uint CasFloatMax32x2(uint op_a,vec2 op_b){return "
                  "packHalf2x16(max(unpackHalf2x16(op_a),op_b));}";
    }
    if (info.uses_atomic_f16x2_add) {
        header += "uint CasFloatAdd16x2(uint op_a,f16vec2 op_b){return "
                  "packFloat2x16(unpackFloat2x16(op_a)+op_b);}";
    }
    if (info.uses_atomic_f16x2_min) {
        header += "uint CasFloatMin16x2(uint op_a,f16vec2 op_b){return "
                  "packFloat2x16(min(unpackFloat2x16(op_a),op_b));}";
    }
    if (info.uses_atomic_f16x2_max) {
        header += "uint CasFloatMax16x2(uint op_a,f16vec2 op_b){return "
                  "packFloat2x16(max(unpackFloat2x16(op_a),op_b));}";
    }
    if (info.uses_atomic_s32_min) {
        header += "uint CasMinS32(uint op_a,uint op_b){return uint(min(int(op_a),int(op_b)));}";
    }
    if (info.uses_atomic_s32_max) {
        header += "uint CasMaxS32(uint op_a,uint op_b){return uint(max(int(op_a),int(op_b)));}";
    }
    if (info.uses_global_memory && profile.support_int64) {
        header += DefineGlobalMemoryFunctions();
    }
    if (info.loads_indexed_attributes) {
        const bool is_array{stage == Stage::Geometry};
        const auto vertex_arg{is_array ? ",uint vertex" : ""};
        std::string func{
            fmt::format("float IndexedAttrLoad(int offset{}){{int base_index=offset>>2;uint "
                        "masked_index=uint(base_index)&3u;switch(base_index>>2){{",
                        vertex_arg)};
        if (info.loads.AnyComponent(IR::Attribute::PositionX)) {
            const auto position_idx{is_array ? "gl_in[vertex]." : ""};
            func += fmt::format("case {}:return {}{}[masked_index];",
                                static_cast<u32>(IR::Attribute::PositionX) >> 2, position_idx,
                                "__out.position");
        }
        const u32 base_attribute_value = static_cast<u32>(IR::Attribute::Generic0X) >> 2;
        for (u32 index = 0; index < IR::NUM_GENERICS; ++index) {
            if (!info.loads.Generic(index)) {
                continue;
            }
            const auto vertex_idx{is_array ? "[vertex]" : ""};
            func += fmt::format("case {}:return in_attr{}{}[masked_index];",
                                base_attribute_value + index, index, vertex_idx);
        }
        func += "default: return 0.0;}}";
        header += func;
    }
    if (info.stores_indexed_attributes) {
        // TODO
    }
}

std::string EmitContext::DefineGlobalMemoryFunctions() {
    const auto define_body{[&](std::string& func, size_t index, std::string_view return_statement) {
        const auto& ssbo{info.storage_buffers_descriptors[index]};
        const u32 size_cbuf_offset{ssbo.cbuf_offset + 8};
        const auto ssbo_addr{fmt::format("ssbo_addr{}", index)};
        const auto cbuf{fmt::format("{}_cbuf{}", stage_name, ssbo.cbuf_index)};
        std::array<std::string, 2> addr_xy;
        std::array<std::string, 2> size_xy;
        for (size_t i = 0; i < addr_xy.size(); ++i) {
            const auto addr_loc{ssbo.cbuf_offset + 4 * i};
            const auto size_loc{size_cbuf_offset + 4 * i};
            addr_xy[i] =
                fmt::format("as_type<uint>({}[{}].{})", cbuf, addr_loc / 16, Swizzle(addr_loc));
            size_xy[i] =
                fmt::format("as_type<uint>({}[{}].{})", cbuf, size_loc / 16, Swizzle(size_loc));
        }
        const u32 ssbo_align_mask{~(static_cast<u32>(profile.min_ssbo_alignment) - 1U)};
        const auto aligned_low_addr{fmt::format("{}&{}", addr_xy[0], ssbo_align_mask)};
        const auto aligned_addr{fmt::format("uvec2({},{})", aligned_low_addr, addr_xy[1])};
        const auto addr_pack{fmt::format("packUint2x32({})", aligned_addr)};
        const auto addr_statement{fmt::format("uint64_t {}={};", ssbo_addr, addr_pack)};
        func += addr_statement;

        const auto size_vec{fmt::format("uvec2({},{})", size_xy[0], size_xy[1])};
        const auto comp_lhs{fmt::format("(addr>={})", ssbo_addr)};
        const auto comp_rhs{fmt::format("(addr<({}+uint64_t({})))", ssbo_addr, size_vec)};
        const auto comparison{fmt::format("if({}&&{}){{", comp_lhs, comp_rhs)};
        func += comparison;

        const auto ssbo_name{fmt::format("{}_ssbo{}", stage_name, index)};
        func += fmt::format(fmt::runtime(return_statement), ssbo_name, ssbo_addr);
    }};
    std::string write_func{"void WriteGlobal32(uint64_t addr,uint data){"};
    std::string write_func_64{"void WriteGlobal64(uint64_t addr,uvec2 data){"};
    std::string write_func_128{"void WriteGlobal128(uint64_t addr,uvec4 data){"};
    std::string load_func{"uint LoadGlobal32(uint64_t addr){"};
    std::string load_func_64{"uvec2 LoadGlobal64(uint64_t addr){"};
    std::string load_func_128{"uvec4 LoadGlobal128(uint64_t addr){"};
    const size_t num_buffers{info.storage_buffers_descriptors.size()};
    for (size_t index = 0; index < num_buffers; ++index) {
        if (!info.nvn_buffer_used[index]) {
            continue;
        }
        define_body(write_func, index, "{0}[uint(addr-{1})>>2]=data;return;}}");
        define_body(write_func_64, index,
                    "{0}[uint(addr-{1})>>2]=data.x;{0}[uint(addr-{1}+4)>>2]=data.y;return;}}");
        define_body(write_func_128, index,
                    "{0}[uint(addr-{1})>>2]=data.x;{0}[uint(addr-{1}+4)>>2]=data.y;{0}[uint("
                    "addr-{1}+8)>>2]=data.z;{0}[uint(addr-{1}+12)>>2]=data.w;return;}}");
        define_body(load_func, index, "return {0}[uint(addr-{1})>>2];}}");
        define_body(load_func_64, index,
                    "return uvec2({0}[uint(addr-{1})>>2],{0}[uint(addr-{1}+4)>>2]);}}");
        define_body(load_func_128, index,
                    "return uvec4({0}[uint(addr-{1})>>2],{0}[uint(addr-{1}+4)>>2],{0}["
                    "uint(addr-{1}+8)>>2],{0}[uint(addr-{1}+12)>>2]);}}");
    }
    write_func += '}';
    write_func_64 += '}';
    write_func_128 += '}';
    load_func += "return 0u;}";
    load_func_64 += "return uint2(0);}";
    load_func_128 += "return uint4(0);}";
    return write_func + write_func_64 + write_func_128 + load_func + load_func_64 + load_func_128;
}

void EmitContext::DefineConstants() {
    if (info.uses_fswzadd) {
        header += "const float FSWZ_A[]=float[4](-1.f,1.f,-1.f,0.f);"
                  "const float FSWZ_B[]=float[4](-1.f,-1.f,1.f,-1.f);";
    }
}

} // namespace Shader::Backend::MSL
