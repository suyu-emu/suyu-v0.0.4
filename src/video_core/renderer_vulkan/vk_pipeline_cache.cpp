// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstddef>
#include <memory>
#include <vector>

#include "common/microprofile.h"
#include "core/core.h"
#include "core/memory.h"
#include "video_core/engines/kepler_compute.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/memory_manager.h"
#include "video_core/renderer_vulkan/fixed_pipeline_state.h"
#include "video_core/renderer_vulkan/maxwell_to_vk.h"
#include "video_core/renderer_vulkan/vk_compute_pipeline.h"
#include "video_core/renderer_vulkan/vk_descriptor_pool.h"
#include "video_core/renderer_vulkan/vk_device.h"
#include "video_core/renderer_vulkan/vk_graphics_pipeline.h"
#include "video_core/renderer_vulkan/vk_pipeline_cache.h"
#include "video_core/renderer_vulkan/vk_rasterizer.h"
#include "video_core/renderer_vulkan/vk_renderpass_cache.h"
#include "video_core/renderer_vulkan/vk_resource_manager.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_update_descriptor.h"
#include "video_core/renderer_vulkan/wrapper.h"
#include "video_core/shader/compiler_settings.h"

namespace Vulkan {

MICROPROFILE_DECLARE(Vulkan_PipelineCache);

using Tegra::Engines::ShaderType;

namespace {

constexpr VkDescriptorType UNIFORM_BUFFER = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
constexpr VkDescriptorType STORAGE_BUFFER = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
constexpr VkDescriptorType UNIFORM_TEXEL_BUFFER = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
constexpr VkDescriptorType COMBINED_IMAGE_SAMPLER = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
constexpr VkDescriptorType STORAGE_IMAGE = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

constexpr VideoCommon::Shader::CompilerSettings compiler_settings{
    VideoCommon::Shader::CompileDepth::FullDecompile};

/// Gets the address for the specified shader stage program
GPUVAddr GetShaderAddress(Core::System& system, Maxwell::ShaderProgram program) {
    const auto& gpu{system.GPU().Maxwell3D()};
    const auto& shader_config{gpu.regs.shader_config[static_cast<std::size_t>(program)]};
    return gpu.regs.code_address.CodeAddress() + shader_config.offset;
}

/// Gets if the current instruction offset is a scheduler instruction
constexpr bool IsSchedInstruction(std::size_t offset, std::size_t main_offset) {
    // Sched instructions appear once every 4 instructions.
    constexpr std::size_t SchedPeriod = 4;
    const std::size_t absolute_offset = offset - main_offset;
    return (absolute_offset % SchedPeriod) == 0;
}

/// Calculates the size of a program stream
std::size_t CalculateProgramSize(const ProgramCode& program, bool is_compute) {
    const std::size_t start_offset = is_compute ? 0 : 10;
    // This is the encoded version of BRA that jumps to itself. All Nvidia
    // shaders end with one.
    constexpr u64 self_jumping_branch = 0xE2400FFFFF07000FULL;
    constexpr u64 mask = 0xFFFFFFFFFF7FFFFFULL;
    std::size_t offset = start_offset;
    while (offset < program.size()) {
        const u64 instruction = program[offset];
        if (!IsSchedInstruction(offset, start_offset)) {
            if ((instruction & mask) == self_jumping_branch) {
                // End on Maxwell's "nop" instruction
                break;
            }
            if (instruction == 0) {
                break;
            }
        }
        ++offset;
    }
    // The last instruction is included in the program size
    return std::min(offset + 1, program.size());
}

/// Gets the shader program code from memory for the specified address
ProgramCode GetShaderCode(Tegra::MemoryManager& memory_manager, const GPUVAddr gpu_addr,
                          const u8* host_ptr, bool is_compute) {
    ProgramCode program_code(VideoCommon::Shader::MAX_PROGRAM_LENGTH);
    ASSERT_OR_EXECUTE(host_ptr != nullptr, {
        std::fill(program_code.begin(), program_code.end(), 0);
        return program_code;
    });
    memory_manager.ReadBlockUnsafe(gpu_addr, program_code.data(),
                                   program_code.size() * sizeof(u64));
    program_code.resize(CalculateProgramSize(program_code, is_compute));
    return program_code;
}

constexpr std::size_t GetStageFromProgram(std::size_t program) {
    return program == 0 ? 0 : program - 1;
}

constexpr ShaderType GetStageFromProgram(Maxwell::ShaderProgram program) {
    return static_cast<ShaderType>(GetStageFromProgram(static_cast<std::size_t>(program)));
}

ShaderType GetShaderType(Maxwell::ShaderProgram program) {
    switch (program) {
    case Maxwell::ShaderProgram::VertexB:
        return ShaderType::Vertex;
    case Maxwell::ShaderProgram::TesselationControl:
        return ShaderType::TesselationControl;
    case Maxwell::ShaderProgram::TesselationEval:
        return ShaderType::TesselationEval;
    case Maxwell::ShaderProgram::Geometry:
        return ShaderType::Geometry;
    case Maxwell::ShaderProgram::Fragment:
        return ShaderType::Fragment;
    default:
        UNIMPLEMENTED_MSG("program={}", static_cast<u32>(program));
        return ShaderType::Vertex;
    }
}

template <VkDescriptorType descriptor_type, class Container>
void AddBindings(std::vector<VkDescriptorSetLayoutBinding>& bindings, u32& binding,
                 VkShaderStageFlags stage_flags, const Container& container) {
    const u32 num_entries = static_cast<u32>(std::size(container));
    for (std::size_t i = 0; i < num_entries; ++i) {
        u32 count = 1;
        if constexpr (descriptor_type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
            // Combined image samplers can be arrayed.
            count = container[i].Size();
        }
        VkDescriptorSetLayoutBinding& entry = bindings.emplace_back();
        entry.binding = binding++;
        entry.descriptorType = descriptor_type;
        entry.descriptorCount = count;
        entry.stageFlags = stage_flags;
        entry.pImmutableSamplers = nullptr;
    }
}

u32 FillDescriptorLayout(const ShaderEntries& entries,
                         std::vector<VkDescriptorSetLayoutBinding>& bindings,
                         Maxwell::ShaderProgram program_type, u32 base_binding) {
    const ShaderType stage = GetStageFromProgram(program_type);
    const VkShaderStageFlags flags = MaxwellToVK::ShaderStage(stage);

    u32 binding = base_binding;
    AddBindings<UNIFORM_BUFFER>(bindings, binding, flags, entries.const_buffers);
    AddBindings<STORAGE_BUFFER>(bindings, binding, flags, entries.global_buffers);
    AddBindings<UNIFORM_TEXEL_BUFFER>(bindings, binding, flags, entries.texel_buffers);
    AddBindings<COMBINED_IMAGE_SAMPLER>(bindings, binding, flags, entries.samplers);
    AddBindings<STORAGE_IMAGE>(bindings, binding, flags, entries.images);
    return binding;
}

} // Anonymous namespace

CachedShader::CachedShader(Core::System& system, Tegra::Engines::ShaderType stage,
                           GPUVAddr gpu_addr, VAddr cpu_addr, ProgramCode program_code,
                           u32 main_offset)
    : RasterizerCacheObject{cpu_addr}, gpu_addr{gpu_addr}, program_code{std::move(program_code)},
      registry{stage, GetEngine(system, stage)}, shader_ir{this->program_code, main_offset,
                                                           compiler_settings, registry},
      entries{GenerateShaderEntries(shader_ir)} {}

CachedShader::~CachedShader() = default;

Tegra::Engines::ConstBufferEngineInterface& CachedShader::GetEngine(
    Core::System& system, Tegra::Engines::ShaderType stage) {
    if (stage == Tegra::Engines::ShaderType::Compute) {
        return system.GPU().KeplerCompute();
    } else {
        return system.GPU().Maxwell3D();
    }
}

VKPipelineCache::VKPipelineCache(Core::System& system, RasterizerVulkan& rasterizer,
                                 const VKDevice& device, VKScheduler& scheduler,
                                 VKDescriptorPool& descriptor_pool,
                                 VKUpdateDescriptorQueue& update_descriptor_queue,
                                 VKRenderPassCache& renderpass_cache)
    : RasterizerCache{rasterizer}, system{system}, device{device}, scheduler{scheduler},
      descriptor_pool{descriptor_pool}, update_descriptor_queue{update_descriptor_queue},
      renderpass_cache{renderpass_cache} {}

VKPipelineCache::~VKPipelineCache() = default;

std::array<Shader, Maxwell::MaxShaderProgram> VKPipelineCache::GetShaders() {
    const auto& gpu = system.GPU().Maxwell3D();

    std::array<Shader, Maxwell::MaxShaderProgram> shaders;
    for (std::size_t index = 0; index < Maxwell::MaxShaderProgram; ++index) {
        const auto program{static_cast<Maxwell::ShaderProgram>(index)};

        // Skip stages that are not enabled
        if (!gpu.regs.IsShaderConfigEnabled(index)) {
            continue;
        }

        auto& memory_manager{system.GPU().MemoryManager()};
        const GPUVAddr program_addr{GetShaderAddress(system, program)};
        const std::optional cpu_addr = memory_manager.GpuToCpuAddress(program_addr);
        ASSERT(cpu_addr);
        auto shader = cpu_addr ? TryGet(*cpu_addr) : null_shader;
        if (!shader) {
            const auto host_ptr{memory_manager.GetPointer(program_addr)};

            // No shader found - create a new one
            constexpr u32 stage_offset = 10;
            const auto stage = static_cast<Tegra::Engines::ShaderType>(index == 0 ? 0 : index - 1);
            auto code = GetShaderCode(memory_manager, program_addr, host_ptr, false);

            shader = std::make_shared<CachedShader>(system, stage, program_addr, *cpu_addr,
                                                    std::move(code), stage_offset);
            if (cpu_addr) {
                Register(shader);
            } else {
                null_shader = shader;
            }
        }
        shaders[index] = std::move(shader);
    }
    return last_shaders = shaders;
}

VKGraphicsPipeline& VKPipelineCache::GetGraphicsPipeline(const GraphicsPipelineCacheKey& key) {
    MICROPROFILE_SCOPE(Vulkan_PipelineCache);

    if (last_graphics_pipeline && last_graphics_key == key) {
        return *last_graphics_pipeline;
    }
    last_graphics_key = key;

    const auto [pair, is_cache_miss] = graphics_cache.try_emplace(key);
    auto& entry = pair->second;
    if (is_cache_miss) {
        LOG_INFO(Render_Vulkan, "Compile 0x{:016X}", key.Hash());
        const auto [program, bindings] = DecompileShaders(key);
        entry = std::make_unique<VKGraphicsPipeline>(device, scheduler, descriptor_pool,
                                                     update_descriptor_queue, renderpass_cache, key,
                                                     bindings, program);
    }
    return *(last_graphics_pipeline = entry.get());
}

VKComputePipeline& VKPipelineCache::GetComputePipeline(const ComputePipelineCacheKey& key) {
    MICROPROFILE_SCOPE(Vulkan_PipelineCache);

    const auto [pair, is_cache_miss] = compute_cache.try_emplace(key);
    auto& entry = pair->second;
    if (!is_cache_miss) {
        return *entry;
    }
    LOG_INFO(Render_Vulkan, "Compile 0x{:016X}", key.Hash());

    auto& memory_manager = system.GPU().MemoryManager();
    const auto program_addr = key.shader;

    const auto cpu_addr = memory_manager.GpuToCpuAddress(program_addr);
    ASSERT(cpu_addr);

    auto shader = cpu_addr ? TryGet(*cpu_addr) : null_kernel;
    if (!shader) {
        // No shader found - create a new one
        const auto host_ptr = memory_manager.GetPointer(program_addr);

        auto code = GetShaderCode(memory_manager, program_addr, host_ptr, true);
        constexpr u32 kernel_main_offset = 0;
        shader = std::make_shared<CachedShader>(system, Tegra::Engines::ShaderType::Compute,
                                                program_addr, *cpu_addr, std::move(code),
                                                kernel_main_offset);
        if (cpu_addr) {
            Register(shader);
        } else {
            null_kernel = shader;
        }
    }

    Specialization specialization;
    specialization.workgroup_size = key.workgroup_size;
    specialization.shared_memory_size = key.shared_memory_size;

    const SPIRVShader spirv_shader{Decompile(device, shader->GetIR(), ShaderType::Compute,
                                             shader->GetRegistry(), specialization),
                                   shader->GetEntries()};
    entry = std::make_unique<VKComputePipeline>(device, scheduler, descriptor_pool,
                                                update_descriptor_queue, spirv_shader);
    return *entry;
}

void VKPipelineCache::Unregister(const Shader& shader) {
    bool finished = false;
    const auto Finish = [&] {
        // TODO(Rodrigo): Instead of finishing here, wait for the fences that use this pipeline and
        // flush.
        if (finished) {
            return;
        }
        finished = true;
        scheduler.Finish();
    };

    const GPUVAddr invalidated_addr = shader->GetGpuAddr();
    for (auto it = graphics_cache.begin(); it != graphics_cache.end();) {
        auto& entry = it->first;
        if (std::find(entry.shaders.begin(), entry.shaders.end(), invalidated_addr) ==
            entry.shaders.end()) {
            ++it;
            continue;
        }
        Finish();
        it = graphics_cache.erase(it);
    }
    for (auto it = compute_cache.begin(); it != compute_cache.end();) {
        auto& entry = it->first;
        if (entry.shader != invalidated_addr) {
            ++it;
            continue;
        }
        Finish();
        it = compute_cache.erase(it);
    }

    RasterizerCache::Unregister(shader);
}

std::pair<SPIRVProgram, std::vector<VkDescriptorSetLayoutBinding>>
VKPipelineCache::DecompileShaders(const GraphicsPipelineCacheKey& key) {
    const auto& fixed_state = key.fixed_state;
    auto& memory_manager = system.GPU().MemoryManager();
    const auto& gpu = system.GPU().Maxwell3D();

    Specialization specialization;
    if (fixed_state.rasterizer.Topology() == Maxwell::PrimitiveTopology::Points) {
        float point_size;
        std::memcpy(&point_size, &fixed_state.rasterizer.point_size, sizeof(float));
        specialization.point_size = point_size;
        ASSERT(point_size != 0.0f);
    }
    for (std::size_t i = 0; i < Maxwell::NumVertexAttributes; ++i) {
        specialization.attribute_types[i] = fixed_state.vertex_input.attributes[i].Type();
    }
    specialization.ndc_minus_one_to_one = fixed_state.rasterizer.ndc_minus_one_to_one;

    SPIRVProgram program;
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    for (std::size_t index = 0; index < Maxwell::MaxShaderProgram; ++index) {
        const auto program_enum = static_cast<Maxwell::ShaderProgram>(index);

        // Skip stages that are not enabled
        if (!gpu.regs.IsShaderConfigEnabled(index)) {
            continue;
        }

        const GPUVAddr gpu_addr = GetShaderAddress(system, program_enum);
        const auto cpu_addr = memory_manager.GpuToCpuAddress(gpu_addr);
        ASSERT(cpu_addr);
        const auto shader = TryGet(*cpu_addr);
        ASSERT(shader);

        const std::size_t stage = index == 0 ? 0 : index - 1; // Stage indices are 0 - 5
        const auto program_type = GetShaderType(program_enum);
        const auto& entries = shader->GetEntries();
        program[stage] = {
            Decompile(device, shader->GetIR(), program_type, shader->GetRegistry(), specialization),
            entries};

        if (program_enum == Maxwell::ShaderProgram::VertexA) {
            // VertexB was combined with VertexA, so we skip the VertexB iteration
            ++index;
        }

        const u32 old_binding = specialization.base_binding;
        specialization.base_binding =
            FillDescriptorLayout(entries, bindings, program_enum, specialization.base_binding);
        ASSERT(old_binding + entries.NumBindings() == specialization.base_binding);
    }
    return {std::move(program), std::move(bindings)};
}

template <VkDescriptorType descriptor_type, class Container>
void AddEntry(std::vector<VkDescriptorUpdateTemplateEntry>& template_entries, u32& binding,
              u32& offset, const Container& container) {
    static constexpr u32 entry_size = static_cast<u32>(sizeof(DescriptorUpdateEntry));
    const u32 count = static_cast<u32>(std::size(container));

    if constexpr (descriptor_type == COMBINED_IMAGE_SAMPLER) {
        for (u32 i = 0; i < count; ++i) {
            const u32 num_samplers = container[i].Size();
            VkDescriptorUpdateTemplateEntry& entry = template_entries.emplace_back();
            entry.dstBinding = binding;
            entry.dstArrayElement = 0;
            entry.descriptorCount = num_samplers;
            entry.descriptorType = descriptor_type;
            entry.offset = offset;
            entry.stride = entry_size;

            ++binding;
            offset += num_samplers * entry_size;
        }
        return;
    }

    if constexpr (descriptor_type == UNIFORM_TEXEL_BUFFER) {
        // Nvidia has a bug where updating multiple uniform texels at once causes the driver to
        // crash.
        for (u32 i = 0; i < count; ++i) {
            VkDescriptorUpdateTemplateEntry& entry = template_entries.emplace_back();
            entry.dstBinding = binding + i;
            entry.dstArrayElement = 0;
            entry.descriptorCount = 1;
            entry.descriptorType = descriptor_type;
            entry.offset = offset + i * entry_size;
            entry.stride = entry_size;
        }
    } else if (count > 0) {
        VkDescriptorUpdateTemplateEntry& entry = template_entries.emplace_back();
        entry.dstBinding = binding;
        entry.dstArrayElement = 0;
        entry.descriptorCount = count;
        entry.descriptorType = descriptor_type;
        entry.offset = offset;
        entry.stride = entry_size;
    }
    offset += count * entry_size;
    binding += count;
}

void FillDescriptorUpdateTemplateEntries(
    const ShaderEntries& entries, u32& binding, u32& offset,
    std::vector<VkDescriptorUpdateTemplateEntryKHR>& template_entries) {
    AddEntry<UNIFORM_BUFFER>(template_entries, offset, binding, entries.const_buffers);
    AddEntry<STORAGE_BUFFER>(template_entries, offset, binding, entries.global_buffers);
    AddEntry<UNIFORM_TEXEL_BUFFER>(template_entries, offset, binding, entries.texel_buffers);
    AddEntry<COMBINED_IMAGE_SAMPLER>(template_entries, offset, binding, entries.samplers);
    AddEntry<STORAGE_IMAGE>(template_entries, offset, binding, entries.images);
}

} // namespace Vulkan
