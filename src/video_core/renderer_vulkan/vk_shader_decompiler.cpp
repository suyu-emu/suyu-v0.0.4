// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <functional>
#include <map>
#include <set>

#include <fmt/format.h>

#include <sirit/sirit.h>

#include "common/alignment.h"
#include "common/assert.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/engines/shader_bytecode.h"
#include "video_core/engines/shader_header.h"
#include "video_core/renderer_vulkan/vk_shader_decompiler.h"
#include "video_core/shader/shader_ir.h"

namespace Vulkan::VKShader {

using Sirit::Id;
using Tegra::Shader::Attribute;
using Tegra::Shader::AttributeUse;
using Tegra::Shader::Register;
using namespace VideoCommon::Shader;

using Maxwell = Tegra::Engines::Maxwell3D::Regs;
using ShaderStage = Tegra::Engines::Maxwell3D::Regs::ShaderStage;

// TODO(Rodrigo): Use rasterizer's value
constexpr u32 MAX_CONSTBUFFER_ELEMENTS = 0x1000;
constexpr u32 STAGE_BINDING_STRIDE = 0x100;

enum class Type { Bool, Bool2, Float, Int, Uint, HalfFloat };

struct SamplerImage {
    Id image_type;
    Id sampled_image_type;
    Id sampler;
};

namespace {

spv::Dim GetSamplerDim(const Sampler& sampler) {
    switch (sampler.GetType()) {
    case Tegra::Shader::TextureType::Texture1D:
        return spv::Dim::Dim1D;
    case Tegra::Shader::TextureType::Texture2D:
        return spv::Dim::Dim2D;
    case Tegra::Shader::TextureType::Texture3D:
        return spv::Dim::Dim3D;
    case Tegra::Shader::TextureType::TextureCube:
        return spv::Dim::Cube;
    default:
        UNIMPLEMENTED_MSG("Unimplemented sampler type={}", static_cast<u32>(sampler.GetType()));
        return spv::Dim::Dim2D;
    }
}

/// Returns true if an attribute index is one of the 32 generic attributes
constexpr bool IsGenericAttribute(Attribute::Index attribute) {
    return attribute >= Attribute::Index::Attribute_0 &&
           attribute <= Attribute::Index::Attribute_31;
}

/// Returns the location of a generic attribute
constexpr u32 GetGenericAttributeLocation(Attribute::Index attribute) {
    ASSERT(IsGenericAttribute(attribute));
    return static_cast<u32>(attribute) - static_cast<u32>(Attribute::Index::Attribute_0);
}

} // namespace

class SPIRVDecompiler : public Sirit::Module {
public:
    explicit SPIRVDecompiler(const ShaderIR& ir, ShaderStage stage)
        : Module(0x00010300), ir{ir}, stage{stage}, header{ir.GetHeader()} {
        AddCapability(spv::Capability::Shader);
        AddExtension("SPV_KHR_storage_buffer_storage_class");
        AddExtension("SPV_KHR_variable_pointers");
    }

    void Decompile() {
        AllocateBindings();
        AllocateLabels();

        DeclareVertex();
        DeclareGeometry();
        DeclareFragment();
        DeclareRegisters();
        DeclarePredicates();
        DeclareLocalMemory();
        DeclareInternalFlags();
        DeclareInputAttributes();
        DeclareOutputAttributes();
        DeclareConstantBuffers();
        DeclareGlobalBuffers();
        DeclareSamplers();

        execute_function =
            Emit(OpFunction(t_void, spv::FunctionControlMask::Inline, TypeFunction(t_void)));
        Emit(OpLabel());

        const u32 first_address = ir.GetBasicBlocks().begin()->first;
        const Id loop_label = OpLabel("loop");
        const Id merge_label = OpLabel("merge");
        const Id dummy_label = OpLabel();
        const Id jump_label = OpLabel();
        continue_label = OpLabel("continue");

        std::vector<Sirit::Literal> literals;
        std::vector<Id> branch_labels;
        for (const auto& pair : labels) {
            const auto [literal, label] = pair;
            literals.push_back(literal);
            branch_labels.push_back(label);
        }

        // TODO(Rodrigo): Figure out the actual depth of the flow stack, for now it seems unlikely
        // that shaders will use 20 nested SSYs and PBKs.
        constexpr u32 FLOW_STACK_SIZE = 20;
        const Id flow_stack_type = TypeArray(t_uint, Constant(t_uint, FLOW_STACK_SIZE));
        jmp_to = Emit(OpVariable(TypePointer(spv::StorageClass::Function, t_uint),
                                 spv::StorageClass::Function, Constant(t_uint, first_address)));
        flow_stack = Emit(OpVariable(TypePointer(spv::StorageClass::Function, flow_stack_type),
                                     spv::StorageClass::Function, ConstantNull(flow_stack_type)));
        flow_stack_top =
            Emit(OpVariable(t_func_uint, spv::StorageClass::Function, Constant(t_uint, 0)));

        Name(jmp_to, "jmp_to");
        Name(flow_stack, "flow_stack");
        Name(flow_stack_top, "flow_stack_top");

        Emit(OpBranch(loop_label));
        Emit(loop_label);
        Emit(OpLoopMerge(merge_label, continue_label, spv::LoopControlMask::Unroll));
        Emit(OpBranch(dummy_label));

        Emit(dummy_label);
        const Id default_branch = OpLabel();
        const Id jmp_to_load = Emit(OpLoad(t_uint, jmp_to));
        Emit(OpSelectionMerge(jump_label, spv::SelectionControlMask::MaskNone));
        Emit(OpSwitch(jmp_to_load, default_branch, literals, branch_labels));

        Emit(default_branch);
        Emit(OpReturn());

        for (const auto& pair : ir.GetBasicBlocks()) {
            const auto& [address, bb] = pair;
            Emit(labels.at(address));

            VisitBasicBlock(bb);

            const auto next_it = labels.lower_bound(address + 1);
            const Id next_label = next_it != labels.end() ? next_it->second : default_branch;
            Emit(OpBranch(next_label));
        }

        Emit(jump_label);
        Emit(OpBranch(continue_label));
        Emit(continue_label);
        Emit(OpBranch(loop_label));
        Emit(merge_label);
        Emit(OpReturn());
        Emit(OpFunctionEnd());
    }

    ShaderEntries GetShaderEntries() const {
        ShaderEntries entries;
        entries.const_buffers_base_binding = const_buffers_base_binding;
        entries.global_buffers_base_binding = global_buffers_base_binding;
        entries.samplers_base_binding = samplers_base_binding;
        for (const auto& cbuf : ir.GetConstantBuffers()) {
            entries.const_buffers.emplace_back(cbuf.second, cbuf.first);
        }
        for (const auto& gmem : ir.GetGlobalMemoryBases()) {
            entries.global_buffers.emplace_back(gmem.cbuf_index, gmem.cbuf_offset);
        }
        for (const auto& sampler : ir.GetSamplers()) {
            entries.samplers.emplace_back(sampler);
        }
        for (const auto& attr : ir.GetInputAttributes()) {
            entries.attributes.insert(GetGenericAttributeLocation(attr.first));
        }
        entries.clip_distances = ir.GetClipDistances();
        entries.shader_length = ir.GetLength();
        entries.entry_function = execute_function;
        entries.interfaces = interfaces;
        return entries;
    }

private:
    static constexpr auto INTERNAL_FLAGS_COUNT = static_cast<std::size_t>(InternalFlag::Amount);
    static constexpr u32 CBUF_STRIDE = 16;

    void AllocateBindings() {
        const u32 binding_base = static_cast<u32>(stage) * STAGE_BINDING_STRIDE;
        u32 binding_iterator = binding_base;

        const auto Allocate = [&binding_iterator](std::size_t count) {
            const u32 current_binding = binding_iterator;
            binding_iterator += static_cast<u32>(count);
            return current_binding;
        };
        const_buffers_base_binding = Allocate(ir.GetConstantBuffers().size());
        global_buffers_base_binding = Allocate(ir.GetGlobalMemoryBases().size());
        samplers_base_binding = Allocate(ir.GetSamplers().size());

        ASSERT_MSG(binding_iterator - binding_base < STAGE_BINDING_STRIDE,
                   "Stage binding stride is too small");
    }

    void AllocateLabels() {
        for (const auto& pair : ir.GetBasicBlocks()) {
            const u32 address = pair.first;
            labels.emplace(address, OpLabel(fmt::format("label_0x{:x}", address)));
        }
    }

    void DeclareVertex() {
        if (stage != ShaderStage::Vertex)
            return;

        DeclareVertexRedeclarations();
    }

    void DeclareGeometry() {
        if (stage != ShaderStage::Geometry)
            return;

        UNIMPLEMENTED();
    }

    void DeclareFragment() {
        if (stage != ShaderStage::Fragment)
            return;

        for (u32 rt = 0; rt < static_cast<u32>(frag_colors.size()); ++rt) {
            if (!IsRenderTargetUsed(rt)) {
                continue;
            }

            const Id id = AddGlobalVariable(OpVariable(t_out_float4, spv::StorageClass::Output));
            Name(id, fmt::format("frag_color{}", rt));
            Decorate(id, spv::Decoration::Location, rt);

            frag_colors[rt] = id;
            interfaces.push_back(id);
        }

        if (header.ps.omap.depth) {
            frag_depth = AddGlobalVariable(OpVariable(t_out_float, spv::StorageClass::Output));
            Name(frag_depth, "frag_depth");
            Decorate(frag_depth, spv::Decoration::BuiltIn,
                     static_cast<u32>(spv::BuiltIn::FragDepth));

            interfaces.push_back(frag_depth);
        }

        frag_coord = DeclareBuiltIn(spv::BuiltIn::FragCoord, spv::StorageClass::Input, t_in_float4,
                                    "frag_coord");
        front_facing = DeclareBuiltIn(spv::BuiltIn::FrontFacing, spv::StorageClass::Input,
                                      t_in_bool, "front_facing");
    }

    void DeclareRegisters() {
        for (const u32 gpr : ir.GetRegisters()) {
            const Id id = OpVariable(t_prv_float, spv::StorageClass::Private, v_float_zero);
            Name(id, fmt::format("gpr_{}", gpr));
            registers.emplace(gpr, AddGlobalVariable(id));
        }
    }

    void DeclarePredicates() {
        for (const auto pred : ir.GetPredicates()) {
            const Id id = OpVariable(t_prv_bool, spv::StorageClass::Private, v_false);
            Name(id, fmt::format("pred_{}", static_cast<u32>(pred)));
            predicates.emplace(pred, AddGlobalVariable(id));
        }
    }

    void DeclareLocalMemory() {
        if (const u64 local_memory_size = header.GetLocalMemorySize(); local_memory_size > 0) {
            const auto element_count = static_cast<u32>(Common::AlignUp(local_memory_size, 4) / 4);
            const Id type_array = TypeArray(t_float, Constant(t_uint, element_count));
            const Id type_pointer = TypePointer(spv::StorageClass::Private, type_array);
            Name(type_pointer, "LocalMemory");

            local_memory =
                OpVariable(type_pointer, spv::StorageClass::Private, ConstantNull(type_array));
            AddGlobalVariable(Name(local_memory, "local_memory"));
        }
    }

    void DeclareInternalFlags() {
        constexpr std::array<const char*, INTERNAL_FLAGS_COUNT> names = {"zero", "sign", "carry",
                                                                         "overflow"};
        for (std::size_t flag = 0; flag < INTERNAL_FLAGS_COUNT; ++flag) {
            const auto flag_code = static_cast<InternalFlag>(flag);
            const Id id = OpVariable(t_prv_bool, spv::StorageClass::Private, v_false);
            internal_flags[flag] = AddGlobalVariable(Name(id, names[flag]));
        }
    }

    void DeclareInputAttributes() {
        for (const auto element : ir.GetInputAttributes()) {
            const Attribute::Index index = element.first;
            if (!IsGenericAttribute(index)) {
                continue;
            }

            UNIMPLEMENTED_IF(stage == ShaderStage::Geometry);

            const u32 location = GetGenericAttributeLocation(index);
            const Id id = OpVariable(t_in_float4, spv::StorageClass::Input);
            Name(AddGlobalVariable(id), fmt::format("in_attr{}", location));
            input_attributes.emplace(index, id);
            interfaces.push_back(id);

            Decorate(id, spv::Decoration::Location, location);

            if (stage != ShaderStage::Fragment) {
                continue;
            }
            switch (header.ps.GetAttributeUse(location)) {
            case AttributeUse::Constant:
                Decorate(id, spv::Decoration::Flat);
                break;
            case AttributeUse::ScreenLinear:
                Decorate(id, spv::Decoration::NoPerspective);
                break;
            case AttributeUse::Perspective:
                // Default
                break;
            default:
                UNREACHABLE_MSG("Unused attribute being fetched");
            }
        }
    }

    void DeclareOutputAttributes() {
        for (const auto index : ir.GetOutputAttributes()) {
            if (!IsGenericAttribute(index)) {
                continue;
            }
            const auto location = GetGenericAttributeLocation(index);
            const Id id = OpVariable(t_out_float4, spv::StorageClass::Output);
            Name(AddGlobalVariable(id), fmt::format("out_attr{}", location));
            output_attributes.emplace(index, id);
            interfaces.push_back(id);

            Decorate(id, spv::Decoration::Location, location);
        }
    }

    void DeclareConstantBuffers() {
        u32 binding = const_buffers_base_binding;
        for (const auto& entry : ir.GetConstantBuffers()) {
            const auto [index, size] = entry;
            const Id id = OpVariable(t_cbuf_ubo, spv::StorageClass::Uniform);
            AddGlobalVariable(Name(id, fmt::format("cbuf_{}", index)));

            Decorate(id, spv::Decoration::Binding, binding++);
            Decorate(id, spv::Decoration::DescriptorSet, DESCRIPTOR_SET);
            constant_buffers.emplace(index, id);
        }
    }

    void DeclareGlobalBuffers() {
        u32 binding = global_buffers_base_binding;
        for (const auto& entry : ir.GetGlobalMemoryBases()) {
            const Id id = OpVariable(t_gmem_ssbo, spv::StorageClass::StorageBuffer);
            AddGlobalVariable(
                Name(id, fmt::format("gmem_{}_{}", entry.cbuf_index, entry.cbuf_offset)));

            Decorate(id, spv::Decoration::Binding, binding++);
            Decorate(id, spv::Decoration::DescriptorSet, DESCRIPTOR_SET);
            global_buffers.emplace(entry, id);
        }
    }

    void DeclareSamplers() {
        u32 binding = samplers_base_binding;
        for (const auto& sampler : ir.GetSamplers()) {
            const auto dim = GetSamplerDim(sampler);
            const int depth = sampler.IsShadow() ? 1 : 0;
            const int arrayed = sampler.IsArray() ? 1 : 0;
            // TODO(Rodrigo): Sampled 1 indicates that the image will be used with a sampler. When
            // SULD and SUST instructions are implemented, replace this value.
            const int sampled = 1;
            const Id image_type =
                TypeImage(t_float, dim, depth, arrayed, false, sampled, spv::ImageFormat::Unknown);
            const Id sampled_image_type = TypeSampledImage(image_type);
            const Id pointer_type =
                TypePointer(spv::StorageClass::UniformConstant, sampled_image_type);
            const Id id = OpVariable(pointer_type, spv::StorageClass::UniformConstant);
            AddGlobalVariable(Name(id, fmt::format("sampler_{}", sampler.GetIndex())));

            sampler_images.insert(
                {static_cast<u32>(sampler.GetIndex()), {image_type, sampled_image_type, id}});

            Decorate(id, spv::Decoration::Binding, binding++);
            Decorate(id, spv::Decoration::DescriptorSet, DESCRIPTOR_SET);
        }
    }

    void DeclareVertexRedeclarations() {
        vertex_index = DeclareBuiltIn(spv::BuiltIn::VertexIndex, spv::StorageClass::Input,
                                      t_in_uint, "vertex_index");
        instance_index = DeclareBuiltIn(spv::BuiltIn::InstanceIndex, spv::StorageClass::Input,
                                        t_in_uint, "instance_index");

        bool is_point_size_declared = false;
        bool is_clip_distances_declared = false;
        for (const auto index : ir.GetOutputAttributes()) {
            if (index == Attribute::Index::PointSize) {
                is_point_size_declared = true;
            } else if (index == Attribute::Index::ClipDistances0123 ||
                       index == Attribute::Index::ClipDistances4567) {
                is_clip_distances_declared = true;
            }
        }

        std::vector<Id> members;
        members.push_back(t_float4);
        if (is_point_size_declared) {
            members.push_back(t_float);
        }
        if (is_clip_distances_declared) {
            members.push_back(TypeArray(t_float, Constant(t_uint, 8)));
        }

        const Id gl_per_vertex_struct = Name(TypeStruct(members), "PerVertex");
        Decorate(gl_per_vertex_struct, spv::Decoration::Block);

        u32 declaration_index = 0;
        const auto MemberDecorateBuiltIn = [&](spv::BuiltIn builtin, std::string name,
                                               bool condition) {
            if (!condition)
                return u32{};
            MemberName(gl_per_vertex_struct, declaration_index, name);
            MemberDecorate(gl_per_vertex_struct, declaration_index, spv::Decoration::BuiltIn,
                           static_cast<u32>(builtin));
            return declaration_index++;
        };

        position_index = MemberDecorateBuiltIn(spv::BuiltIn::Position, "position", true);
        point_size_index =
            MemberDecorateBuiltIn(spv::BuiltIn::PointSize, "point_size", is_point_size_declared);
        clip_distances_index = MemberDecorateBuiltIn(spv::BuiltIn::ClipDistance, "clip_distances",
                                                     is_clip_distances_declared);

        const Id type_pointer = TypePointer(spv::StorageClass::Output, gl_per_vertex_struct);
        per_vertex = OpVariable(type_pointer, spv::StorageClass::Output);
        AddGlobalVariable(Name(per_vertex, "per_vertex"));
        interfaces.push_back(per_vertex);
    }

    void VisitBasicBlock(const NodeBlock& bb) {
        for (const Node node : bb) {
            static_cast<void>(Visit(node));
        }
    }

    Id Visit(Node node) {
        if (const auto operation = std::get_if<OperationNode>(node)) {
            UNIMPLEMENTED();

        } else if (const auto gpr = std::get_if<GprNode>(node)) {
            UNIMPLEMENTED();

        } else if (const auto immediate = std::get_if<ImmediateNode>(node)) {
            UNIMPLEMENTED();

        } else if (const auto predicate = std::get_if<PredicateNode>(node)) {
            UNIMPLEMENTED();

        } else if (const auto abuf = std::get_if<AbufNode>(node)) {
            UNIMPLEMENTED();

        } else if (const auto cbuf = std::get_if<CbufNode>(node)) {
            UNIMPLEMENTED();

        } else if (const auto gmem = std::get_if<GmemNode>(node)) {
            UNIMPLEMENTED();

        } else if (const auto conditional = std::get_if<ConditionalNode>(node)) {
            UNIMPLEMENTED();

        } else if (const auto comment = std::get_if<CommentNode>(node)) {
            Name(Emit(OpUndef(t_void)), comment->GetText());
            return {};
        }

        UNREACHABLE();
        return {};
    }

    Id DeclareBuiltIn(spv::BuiltIn builtin, spv::StorageClass storage, Id type,
                      const std::string& name) {
        const Id id = OpVariable(type, storage);
        Decorate(id, spv::Decoration::BuiltIn, static_cast<u32>(builtin));
        AddGlobalVariable(Name(id, name));
        interfaces.push_back(id);
        return id;
    }

    bool IsRenderTargetUsed(u32 rt) const {
        for (u32 component = 0; component < 4; ++component) {
            if (header.ps.IsColorComponentOutputEnabled(rt, component)) {
                return true;
            }
        }
        return false;
    }

    const ShaderIR& ir;
    const ShaderStage stage;
    const Tegra::Shader::Header header;

    const Id t_void = Name(TypeVoid(), "void");

    const Id t_bool = Name(TypeBool(), "bool");
    const Id t_bool2 = Name(TypeVector(t_bool, 2), "bool2");

    const Id t_int = Name(TypeInt(32, true), "int");
    const Id t_int2 = Name(TypeVector(t_int, 2), "int2");
    const Id t_int3 = Name(TypeVector(t_int, 3), "int3");
    const Id t_int4 = Name(TypeVector(t_int, 4), "int4");

    const Id t_uint = Name(TypeInt(32, false), "uint");
    const Id t_uint2 = Name(TypeVector(t_uint, 2), "uint2");
    const Id t_uint3 = Name(TypeVector(t_uint, 3), "uint3");
    const Id t_uint4 = Name(TypeVector(t_uint, 4), "uint4");

    const Id t_float = Name(TypeFloat(32), "float");
    const Id t_float2 = Name(TypeVector(t_float, 2), "float2");
    const Id t_float3 = Name(TypeVector(t_float, 3), "float3");
    const Id t_float4 = Name(TypeVector(t_float, 4), "float4");

    const Id t_prv_bool = Name(TypePointer(spv::StorageClass::Private, t_bool), "prv_bool");
    const Id t_prv_float = Name(TypePointer(spv::StorageClass::Private, t_float), "prv_float");

    const Id t_func_uint = Name(TypePointer(spv::StorageClass::Function, t_uint), "func_uint");

    const Id t_in_bool = Name(TypePointer(spv::StorageClass::Input, t_bool), "in_bool");
    const Id t_in_uint = Name(TypePointer(spv::StorageClass::Input, t_uint), "in_uint");
    const Id t_in_float = Name(TypePointer(spv::StorageClass::Input, t_float), "in_float");
    const Id t_in_float4 = Name(TypePointer(spv::StorageClass::Input, t_float4), "in_float4");

    const Id t_out_float = Name(TypePointer(spv::StorageClass::Output, t_float), "out_float");
    const Id t_out_float4 = Name(TypePointer(spv::StorageClass::Output, t_float4), "out_float4");

    const Id t_cbuf_float = TypePointer(spv::StorageClass::Uniform, t_float);
    const Id t_cbuf_array =
        Decorate(Name(TypeArray(t_float4, Constant(t_uint, MAX_CONSTBUFFER_ELEMENTS)), "CbufArray"),
                 spv::Decoration::ArrayStride, CBUF_STRIDE);
    const Id t_cbuf_struct = MemberDecorate(
        Decorate(TypeStruct(t_cbuf_array), spv::Decoration::Block), 0, spv::Decoration::Offset, 0);
    const Id t_cbuf_ubo = TypePointer(spv::StorageClass::Uniform, t_cbuf_struct);

    const Id t_gmem_float = TypePointer(spv::StorageClass::StorageBuffer, t_float);
    const Id t_gmem_array =
        Name(Decorate(TypeRuntimeArray(t_float), spv::Decoration::ArrayStride, 4u), "GmemArray");
    const Id t_gmem_struct = MemberDecorate(
        Decorate(TypeStruct(t_gmem_array), spv::Decoration::Block), 0, spv::Decoration::Offset, 0);
    const Id t_gmem_ssbo = TypePointer(spv::StorageClass::StorageBuffer, t_gmem_struct);

    const Id v_float_zero = Constant(t_float, 0.0f);
    const Id v_true = ConstantTrue(t_bool);
    const Id v_false = ConstantFalse(t_bool);

    Id per_vertex{};
    std::map<u32, Id> registers;
    std::map<Tegra::Shader::Pred, Id> predicates;
    Id local_memory{};
    std::array<Id, INTERNAL_FLAGS_COUNT> internal_flags{};
    std::map<Attribute::Index, Id> input_attributes;
    std::map<Attribute::Index, Id> output_attributes;
    std::map<u32, Id> constant_buffers;
    std::map<GlobalMemoryBase, Id> global_buffers;
    std::map<u32, SamplerImage> sampler_images;

    Id instance_index{};
    Id vertex_index{};
    std::array<Id, Maxwell::NumRenderTargets> frag_colors{};
    Id frag_depth{};
    Id frag_coord{};
    Id front_facing{};

    u32 position_index{};
    u32 point_size_index{};
    u32 clip_distances_index{};

    std::vector<Id> interfaces;

    u32 const_buffers_base_binding{};
    u32 global_buffers_base_binding{};
    u32 samplers_base_binding{};

    Id execute_function{};
    Id jmp_to{};
    Id flow_stack_top{};
    Id flow_stack{};
    Id continue_label{};
    std::map<u32, Id> labels;
};

DecompilerResult Decompile(const VideoCommon::Shader::ShaderIR& ir, Maxwell::ShaderStage stage) {
    auto decompiler = std::make_unique<SPIRVDecompiler>(ir, stage);
    decompiler->Decompile();
    return {std::move(decompiler), decompiler->GetShaderEntries()};
}

} // namespace Vulkan::VKShader
