// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <string>
#include <tuple>
#include <type_traits>

#include "common/div_ceil.h"
#include "common/settings.h"
#include "shader_recompiler/backend/msl/emit_msl.h"
#include "shader_recompiler/backend/msl/emit_msl_instructions.h"
#include "shader_recompiler/backend/msl/msl_emit_context.h"
#include "shader_recompiler/frontend/ir/ir_emitter.h"

namespace Shader::Backend::MSL {
namespace {
template <class Func>
struct FuncTraits {};

template <class ReturnType_, class... Args>
struct FuncTraits<ReturnType_ (*)(Args...)> {
    using ReturnType = ReturnType_;

    static constexpr size_t NUM_ARGS = sizeof...(Args);

    template <size_t I>
    using ArgType = std::tuple_element_t<I, std::tuple<Args...>>;
};

template <auto func, typename... Args>
void SetDefinition(EmitContext& ctx, IR::Inst* inst, Args... args) {
    inst->SetDefinition<Id>(func(ctx, std::forward<Args>(args)...));
}

template <typename ArgType>
auto Arg(EmitContext& ctx, const IR::Value& arg) {
    if constexpr (std::is_same_v<ArgType, std::string_view>) {
        return ctx.var_alloc.Consume(arg);
    } else if constexpr (std::is_same_v<ArgType, const IR::Value&>) {
        return arg;
    } else if constexpr (std::is_same_v<ArgType, u32>) {
        return arg.U32();
    } else if constexpr (std::is_same_v<ArgType, IR::Attribute>) {
        return arg.Attribute();
    } else if constexpr (std::is_same_v<ArgType, IR::Patch>) {
        return arg.Patch();
    } else if constexpr (std::is_same_v<ArgType, IR::Reg>) {
        return arg.Reg();
    }
}

template <auto func, bool is_first_arg_inst, size_t... I>
void Invoke(EmitContext& ctx, IR::Inst* inst, std::index_sequence<I...>) {
    using Traits = FuncTraits<decltype(func)>;
    if constexpr (std::is_same_v<typename Traits::ReturnType, Id>) {
        if constexpr (is_first_arg_inst) {
            SetDefinition<func>(
                ctx, inst, *inst,
                Arg<typename Traits::template ArgType<I + 2>>(ctx, inst->Arg(I))...);
        } else {
            SetDefinition<func>(
                ctx, inst, Arg<typename Traits::template ArgType<I + 1>>(ctx, inst->Arg(I))...);
        }
    } else {
        if constexpr (is_first_arg_inst) {
            func(ctx, *inst, Arg<typename Traits::template ArgType<I + 2>>(ctx, inst->Arg(I))...);
        } else {
            func(ctx, Arg<typename Traits::template ArgType<I + 1>>(ctx, inst->Arg(I))...);
        }
    }
}

template <auto func>
void Invoke(EmitContext& ctx, IR::Inst* inst) {
    using Traits = FuncTraits<decltype(func)>;
    static_assert(Traits::NUM_ARGS >= 1, "Insufficient arguments");
    if constexpr (Traits::NUM_ARGS == 1) {
        Invoke<func, false>(ctx, inst, std::make_index_sequence<0>{});
    } else {
        using FirstArgType = typename Traits::template ArgType<1>;
        static constexpr bool is_first_arg_inst = std::is_same_v<FirstArgType, IR::Inst&>;
        using Indices = std::make_index_sequence<Traits::NUM_ARGS - (is_first_arg_inst ? 2 : 1)>;
        Invoke<func, is_first_arg_inst>(ctx, inst, Indices{});
    }
}

void EmitInst(EmitContext& ctx, IR::Inst* inst) {
    switch (inst->GetOpcode()) {
#define OPCODE(name, result_type, ...)                                                             \
    case IR::Opcode::name:                                                                         \
        return Invoke<&Emit##name>(ctx, inst);
#include "shader_recompiler/frontend/ir/opcodes.inc"
#undef OPCODE
    }
    throw LogicError("Invalid opcode {}", inst->GetOpcode());
}

bool IsReference(IR::Inst& inst) {
    return inst.GetOpcode() == IR::Opcode::Reference;
}

void PrecolorInst(IR::Inst& phi) {
    // Insert phi moves before references to avoid overwriting other phis
    const size_t num_args{phi.NumArgs()};
    for (size_t i = 0; i < num_args; ++i) {
        IR::Block& phi_block{*phi.PhiBlock(i)};
        auto it{std::find_if_not(phi_block.rbegin(), phi_block.rend(), IsReference).base()};
        IR::IREmitter ir{phi_block, it};
        const IR::Value arg{phi.Arg(i)};
        if (arg.IsImmediate()) {
            ir.PhiMove(phi, arg);
        } else {
            ir.PhiMove(phi, IR::Value{arg.InstRecursive()});
        }
    }
    for (size_t i = 0; i < num_args; ++i) {
        IR::IREmitter{*phi.PhiBlock(i)}.Reference(IR::Value{&phi});
    }
}

void Precolor(const IR::Program& program) {
    for (IR::Block* const block : program.blocks) {
        for (IR::Inst& phi : block->Instructions()) {
            if (!IR::IsPhi(phi)) {
                break;
            }
            PrecolorInst(phi);
        }
    }
}

void EmitCode(EmitContext& ctx, const IR::Program& program) {
    for (const IR::AbstractSyntaxNode& node : program.syntax_list) {
        switch (node.type) {
        case IR::AbstractSyntaxNode::Type::Block:
            for (IR::Inst& inst : node.data.block->Instructions()) {
                EmitInst(ctx, &inst);
            }
            break;
        case IR::AbstractSyntaxNode::Type::If:
            ctx.Add("if({}){{", ctx.var_alloc.Consume(node.data.if_node.cond));
            break;
        case IR::AbstractSyntaxNode::Type::EndIf:
            ctx.Add("}}");
            break;
        case IR::AbstractSyntaxNode::Type::Break:
            if (node.data.break_node.cond.IsImmediate()) {
                if (node.data.break_node.cond.U1()) {
                    ctx.Add("break;");
                }
            } else {
                ctx.Add("if({}){{break;}}", ctx.var_alloc.Consume(node.data.break_node.cond));
            }
            break;
        case IR::AbstractSyntaxNode::Type::Return:
        case IR::AbstractSyntaxNode::Type::Unreachable:
            ctx.Add("return;");
            break;
        case IR::AbstractSyntaxNode::Type::Loop:
            ctx.Add("for(;;){{");
            break;
        case IR::AbstractSyntaxNode::Type::Repeat:
            if (Settings::values.disable_shader_loop_safety_checks) {
                ctx.Add("if(!{}){{break;}}}}", ctx.var_alloc.Consume(node.data.repeat.cond));
            } else {
                ctx.Add("if(--loop{}<0 || !{}){{break;}}}}", ctx.num_safety_loop_vars++,
                        ctx.var_alloc.Consume(node.data.repeat.cond));
            }
            break;
        default:
            throw NotImplementedException("AbstractSyntaxNode Type {}", node.type);
        }
    }
}

bool IsPreciseType(MslVarType type) {
    switch (type) {
    case MslVarType::PrecF32:
    case MslVarType::PrecF64:
        return true;
    default:
        return false;
    }
}

void DefineVariables(const EmitContext& ctx, std::string& header) {
    for (u32 i = 0; i < static_cast<u32>(MslVarType::Void); ++i) {
        const auto type{static_cast<MslVarType>(i)};
        const auto& tracker{ctx.var_alloc.GetUseTracker(type)};
        const auto type_name{ctx.var_alloc.GetMslType(type)};
        const bool has_precise_bug{ctx.stage == Stage::Fragment && ctx.profile.has_gl_precise_bug};
        const auto precise{!has_precise_bug && IsPreciseType(type) ? "precise " : ""};
        // Temps/return types that are never used are stored at index 0
        if (tracker.uses_temp) {
            header += fmt::format("{}{} t{}={}(0);", precise, type_name,
                                  ctx.var_alloc.Representation(0, type), type_name);
        }
        for (u32 index = 0; index < tracker.num_used; ++index) {
            header += fmt::format("{}{} {}={}(0);", precise, type_name,
                                  ctx.var_alloc.Representation(index, type), type_name);
        }
    }
    for (u32 i = 0; i < ctx.num_safety_loop_vars; ++i) {
        header += fmt::format("int loop{}=0x2000;", i);
    }
}

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

void DefineInputs(EmitContext& ctx, std::string& header, Bindings& bindings) {
    bool added{false};

    // Constant buffers
    for (const auto& desc : ctx.info.constant_buffer_descriptors) {
        const u32 cbuf_used_size{
            Common::DivCeil(ctx.info.constant_buffer_used_sizes[desc.index], 16U)};
        const u32 cbuf_binding_size{ctx.info.uses_global_memory ? 0x1000U : cbuf_used_size};
        if (added)
            header += ",";
        header += fmt::format("constant float4& cbuf{}[{}] [[buffer({})]]", desc.index,
                              cbuf_binding_size, bindings.uniform_buffer);
        bindings.uniform_buffer += desc.count;
        added = true;
    }

    // Constant buffer indirect
    // TODO

    // Storage space buffers
    u32 index{};
    for (const auto& desc : ctx.info.storage_buffers_descriptors) {
        if (added)
            header += ",";
        header +=
            fmt::format("device uint& ssbo{}[] [[buffer({})]]", index, bindings.storage_buffer);
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
        header += fmt::format("layout(binding={}{}) uniform {}uimageBuffer img{}{};",
                              bindings.image, format, qualifier, bindings.image, array_decorator);
        bindings.image += desc.count;
    }
    */
    ctx.images.reserve(ctx.info.image_descriptors.size());
    for (const auto& desc : ctx.info.image_descriptors) {
        ctx.images.push_back({bindings.image, desc.count});
        // TODO: do we need format?
        // const auto format{ImageFormatString(desc.format)};
        const auto image_type{ImageType(desc.type)};
        const auto qualifier{ImageAccessQualifier(desc.is_written, desc.is_read)};
        const auto array_decorator{desc.count > 1 ? fmt::format("[{}]", desc.count) : ""};
        if (added)
            header += ",";
        header += fmt::format("{}<{}> img{}{} [[texture({})]]", qualifier, image_type,
                              bindings.image, array_decorator, bindings.image);
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
        header += fmt::format("layout(binding={}) uniform {} tex{}{};", bindings.texture,
                              sampler_type, bindings.texture, array_decorator);
        bindings.texture += desc.count;
    }
    */
    ctx.textures.reserve(ctx.info.texture_descriptors.size());
    for (const auto& desc : ctx.info.texture_descriptors) {
        ctx.textures.push_back({bindings.texture, desc.count});
        const auto texture_type{desc.is_depth ? DepthSamplerType(desc.type)
                                              : ColorSamplerType(desc.type, desc.is_multisample)};
        const auto array_decorator{desc.count > 1 ? fmt::format("[{}]", desc.count) : ""};
        if (added)
            header += ",";
        header += fmt::format("{} tex{}{} [[texture({})]]", texture_type, bindings.texture,
                              array_decorator, bindings.texture);
        header += fmt::format(",sampler samp{}{} [[sampler({})]]", bindings.texture,
                              array_decorator, bindings.texture);
        bindings.texture += desc.count;
        added = true;
    }
}
} // Anonymous namespace

std::string EmitMSL(const Profile& profile, const RuntimeInfo& runtime_info, IR::Program& program,
                    Bindings& bindings) {
    EmitContext ctx{program, bindings, profile, runtime_info};
    std::string inputs;
    DefineInputs(ctx, inputs, bindings);
    Precolor(program);
    EmitCode(ctx, program);
    ctx.header.insert(0, "#include <metal_stdlib>\nusing namespace metal;\n");
    if (program.shared_memory_size > 0) {
        const auto requested_size{program.shared_memory_size};
        const auto max_size{profile.gl_max_compute_smem_size};
        const bool needs_clamp{requested_size > max_size};
        if (needs_clamp) {
            LOG_WARNING(Shader_MSL, "Requested shared memory size ({}) exceeds device limit ({})",
                        requested_size, max_size);
        }
        const auto smem_size{needs_clamp ? max_size : requested_size};
        ctx.header += fmt::format("shared uint smem[{}];", Common::DivCeil(smem_size, 4U));
    }
    ctx.header += "void main_(";
    ctx.header += inputs;
    ctx.header += "){\n";
    if (program.local_memory_size > 0) {
        ctx.header += fmt::format("uint lmem[{}];", Common::DivCeil(program.local_memory_size, 4U));
    }
    DefineVariables(ctx, ctx.header);
    if (ctx.uses_cc_carry) {
        ctx.header += "uint carry;";
    }
    if (program.info.uses_subgroup_shuffles) {
        ctx.header += "bool shfl_in_bounds;";
        ctx.header += "uint shfl_result;";
    }
    ctx.code.insert(0, ctx.header);
    ctx.code += '}';
    return ctx.code;
}

} // namespace Shader::Backend::MSL
