// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_set>
#include <boost/functional/hash.hpp>
#include "common/assert.h"
#include "common/scope_exit.h"
#include "core/core.h"
#include "core/frontend/emu_window.h"
#include "video_core/engines/kepler_compute.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/memory_manager.h"
#include "video_core/renderer_opengl/gl_rasterizer.h"
#include "video_core/renderer_opengl/gl_shader_cache.h"
#include "video_core/renderer_opengl/gl_shader_decompiler.h"
#include "video_core/renderer_opengl/gl_shader_disk_cache.h"
#include "video_core/renderer_opengl/utils.h"
#include "video_core/shader/shader_ir.h"

namespace OpenGL {

using Tegra::Engines::ShaderType;
using VideoCommon::Shader::ConstBufferLocker;
using VideoCommon::Shader::ProgramCode;
using VideoCommon::Shader::ShaderIR;

namespace {

// One UBO is always reserved for emulation values on staged shaders
constexpr u32 STAGE_RESERVED_UBOS = 1;

constexpr u32 STAGE_MAIN_OFFSET = 10;
constexpr u32 KERNEL_MAIN_OFFSET = 0;

constexpr VideoCommon::Shader::CompilerSettings COMPILER_SETTINGS{};

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
std::size_t CalculateProgramSize(const GLShader::ProgramCode& program) {
    constexpr std::size_t start_offset = 10;
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
        offset++;
    }
    // The last instruction is included in the program size
    return std::min(offset + 1, program.size());
}

/// Gets the shader program code from memory for the specified address
ProgramCode GetShaderCode(Tegra::MemoryManager& memory_manager, const GPUVAddr gpu_addr,
                          const u8* host_ptr) {
    ProgramCode program_code(VideoCommon::Shader::MAX_PROGRAM_LENGTH);
    ASSERT_OR_EXECUTE(host_ptr != nullptr, {
        std::fill(program_code.begin(), program_code.end(), 0);
        return program_code;
    });
    memory_manager.ReadBlockUnsafe(gpu_addr, program_code.data(),
                                   program_code.size() * sizeof(u64));
    program_code.resize(CalculateProgramSize(program_code));
    return program_code;
}

/// Gets the shader type from a Maxwell program type
constexpr GLenum GetShaderType(ProgramType program_type) {
    switch (program_type) {
    case ProgramType::VertexA:
    case ProgramType::VertexB:
        return GL_VERTEX_SHADER;
    case ProgramType::Geometry:
        return GL_GEOMETRY_SHADER;
    case ProgramType::Fragment:
        return GL_FRAGMENT_SHADER;
    case ProgramType::Compute:
        return GL_COMPUTE_SHADER;
    default:
        return GL_NONE;
    }
}

/// Describes primitive behavior on geometry shaders
constexpr std::tuple<const char*, const char*, u32> GetPrimitiveDescription(GLenum primitive_mode) {
    switch (primitive_mode) {
    case GL_POINTS:
        return {"points", "Points", 1};
    case GL_LINES:
    case GL_LINE_STRIP:
        return {"lines", "Lines", 2};
    case GL_LINES_ADJACENCY:
    case GL_LINE_STRIP_ADJACENCY:
        return {"lines_adjacency", "LinesAdj", 4};
    case GL_TRIANGLES:
    case GL_TRIANGLE_STRIP:
    case GL_TRIANGLE_FAN:
        return {"triangles", "Triangles", 3};
    case GL_TRIANGLES_ADJACENCY:
    case GL_TRIANGLE_STRIP_ADJACENCY:
        return {"triangles_adjacency", "TrianglesAdj", 6};
    default:
        return {"points", "Invalid", 1};
    }
}

ProgramType GetProgramType(Maxwell::ShaderProgram program) {
    switch (program) {
    case Maxwell::ShaderProgram::VertexA:
        return ProgramType::VertexA;
    case Maxwell::ShaderProgram::VertexB:
        return ProgramType::VertexB;
    case Maxwell::ShaderProgram::TesselationControl:
        return ProgramType::TessellationControl;
    case Maxwell::ShaderProgram::TesselationEval:
        return ProgramType::TessellationEval;
    case Maxwell::ShaderProgram::Geometry:
        return ProgramType::Geometry;
    case Maxwell::ShaderProgram::Fragment:
        return ProgramType::Fragment;
    }
    UNREACHABLE();
    return {};
}

/// Hashes one (or two) program streams
u64 GetUniqueIdentifier(ProgramType program_type, const ProgramCode& code,
                        const ProgramCode& code_b) {
    u64 unique_identifier = boost::hash_value(code);
    if (program_type == ProgramType::VertexA) {
        // VertexA programs include two programs
        boost::hash_combine(unique_identifier, boost::hash_value(code_b));
    }
    return unique_identifier;
}

/// Creates an unspecialized program from code streams
std::string GenerateGLSL(const Device& device, ProgramType program_type, const ShaderIR& ir,
                         const std::optional<ShaderIR>& ir_b) {
    switch (program_type) {
    case ProgramType::VertexA:
    case ProgramType::VertexB:
        return GLShader::GenerateVertexShader(device, ir, ir_b ? &*ir_b : nullptr);
    case ProgramType::Geometry:
        return GLShader::GenerateGeometryShader(device, ir);
    case ProgramType::Fragment:
        return GLShader::GenerateFragmentShader(device, ir);
    case ProgramType::Compute:
        return GLShader::GenerateComputeShader(device, ir);
    default:
        UNIMPLEMENTED_MSG("Unimplemented program_type={}", static_cast<u32>(program_type));
        return {};
    }
}

constexpr const char* GetProgramTypeName(ProgramType program_type) {
    switch (program_type) {
    case ProgramType::VertexA:
    case ProgramType::VertexB:
        return "VS";
    case ProgramType::TessellationControl:
        return "TCS";
    case ProgramType::TessellationEval:
        return "TES";
    case ProgramType::Geometry:
        return "GS";
    case ProgramType::Fragment:
        return "FS";
    case ProgramType::Compute:
        return "CS";
    }
    return "UNK";
}

Tegra::Engines::ShaderType GetEnginesShaderType(ProgramType program_type) {
    switch (program_type) {
    case ProgramType::VertexA:
    case ProgramType::VertexB:
        return Tegra::Engines::ShaderType::Vertex;
    case ProgramType::TessellationControl:
        return Tegra::Engines::ShaderType::TesselationControl;
    case ProgramType::TessellationEval:
        return Tegra::Engines::ShaderType::TesselationEval;
    case ProgramType::Geometry:
        return Tegra::Engines::ShaderType::Geometry;
    case ProgramType::Fragment:
        return Tegra::Engines::ShaderType::Fragment;
    case ProgramType::Compute:
        return Tegra::Engines::ShaderType::Compute;
    }
    UNREACHABLE();
    return {};
}

std::string GetShaderId(u64 unique_identifier, ProgramType program_type) {
    return fmt::format("{}{:016X}", GetProgramTypeName(program_type), unique_identifier);
}

Tegra::Engines::ConstBufferEngineInterface& GetConstBufferEngineInterface(
    Core::System& system, ProgramType program_type) {
    if (program_type == ProgramType::Compute) {
        return system.GPU().KeplerCompute();
    } else {
        return system.GPU().Maxwell3D();
    }
}

std::unique_ptr<ConstBufferLocker> MakeLocker(Core::System& system, ProgramType program_type) {
    return std::make_unique<ConstBufferLocker>(GetEnginesShaderType(program_type),
                                               GetConstBufferEngineInterface(system, program_type));
}

void FillLocker(ConstBufferLocker& locker, const ShaderDiskCacheUsage& usage) {
    for (const auto& key : usage.keys) {
        const auto [buffer, offset] = key.first;
        locker.InsertKey(buffer, offset, key.second);
    }
    for (const auto& [offset, sampler] : usage.bound_samplers) {
        locker.InsertBoundSampler(offset, sampler);
    }
    for (const auto& [key, sampler] : usage.bindless_samplers) {
        const auto [buffer, offset] = key;
        locker.InsertBindlessSampler(buffer, offset, sampler);
    }
}

CachedProgram BuildShader(const Device& device, u64 unique_identifier, ProgramType program_type,
                          const ProgramCode& program_code, const ProgramCode& program_code_b,
                          const ProgramVariant& variant, ConstBufferLocker& locker,
                          bool hint_retrievable = false) {
    LOG_INFO(Render_OpenGL, "called. {}", GetShaderId(unique_identifier, program_type));

    const bool is_compute = program_type == ProgramType::Compute;
    const u32 main_offset = is_compute ? KERNEL_MAIN_OFFSET : STAGE_MAIN_OFFSET;
    const ShaderIR ir(program_code, main_offset, COMPILER_SETTINGS, locker);
    std::optional<ShaderIR> ir_b;
    if (!program_code_b.empty()) {
        ir_b.emplace(program_code_b, main_offset, COMPILER_SETTINGS, locker);
    }
    const auto entries = GLShader::GetEntries(ir);

    auto base_bindings{variant.base_bindings};
    const auto primitive_mode{variant.primitive_mode};
    const auto texture_buffer_usage{variant.texture_buffer_usage};

    std::string source = fmt::format(R"(// {}
#version 430 core
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shader_viewport_layer_array : enable
#extension GL_EXT_shader_image_load_formatted : enable
#extension GL_NV_gpu_shader5 : enable
#extension GL_NV_shader_thread_group : enable
#extension GL_NV_shader_thread_shuffle : enable
)",
                                     GetShaderId(unique_identifier, program_type));
    if (is_compute) {
        source += "#extension GL_ARB_compute_variable_group_size : require\n";
    }
    source += '\n';

    if (!is_compute) {
        source += fmt::format("#define EMULATION_UBO_BINDING {}\n", base_bindings.cbuf++);
    }

    for (const auto& cbuf : entries.const_buffers) {
        source +=
            fmt::format("#define CBUF_BINDING_{} {}\n", cbuf.GetIndex(), base_bindings.cbuf++);
    }
    for (const auto& gmem : entries.global_memory_entries) {
        source += fmt::format("#define GMEM_BINDING_{}_{} {}\n", gmem.GetCbufIndex(),
                              gmem.GetCbufOffset(), base_bindings.gmem++);
    }
    for (const auto& sampler : entries.samplers) {
        source += fmt::format("#define SAMPLER_BINDING_{} {}\n", sampler.GetIndex(),
                              base_bindings.sampler++);
    }
    for (const auto& image : entries.images) {
        source +=
            fmt::format("#define IMAGE_BINDING_{} {}\n", image.GetIndex(), base_bindings.image++);
    }

    // Transform 1D textures to texture samplers by declaring its preprocessor macros.
    for (std::size_t i = 0; i < texture_buffer_usage.size(); ++i) {
        if (!texture_buffer_usage.test(i)) {
            continue;
        }
        source += fmt::format("#define SAMPLER_{}_IS_BUFFER\n", i);
    }
    if (texture_buffer_usage.any()) {
        source += '\n';
    }

    if (program_type == ProgramType::Geometry) {
        const auto [glsl_topology, debug_name, max_vertices] =
            GetPrimitiveDescription(primitive_mode);

        source += "layout (" + std::string(glsl_topology) + ") in;\n\n";
        source += "#define MAX_VERTEX_INPUT " + std::to_string(max_vertices) + '\n';
    }
    if (program_type == ProgramType::Compute) {
        source += "layout (local_size_variable) in;\n";
    }

    source += '\n';
    source += GenerateGLSL(device, program_type, ir, ir_b);

    OGLShader shader;
    shader.Create(source.c_str(), GetShaderType(program_type));

    auto program = std::make_shared<OGLProgram>();
    program->Create(true, hint_retrievable, shader.handle);
    return program;
}

std::unordered_set<GLenum> GetSupportedFormats() {
    GLint num_formats{};
    glGetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS, &num_formats);

    std::vector<GLint> formats(num_formats);
    glGetIntegerv(GL_PROGRAM_BINARY_FORMATS, formats.data());

    std::unordered_set<GLenum> supported_formats;
    for (const GLint format : formats) {
        supported_formats.insert(static_cast<GLenum>(format));
    }
    return supported_formats;
}

} // Anonymous namespace

CachedShader::CachedShader(const ShaderParameters& params, ProgramType program_type,
                           GLShader::ShaderEntries entries, ProgramCode program_code,
                           ProgramCode program_code_b)
    : RasterizerCacheObject{params.host_ptr}, system{params.system},
      disk_cache{params.disk_cache}, device{params.device}, cpu_addr{params.cpu_addr},
      unique_identifier{params.unique_identifier}, program_type{program_type}, entries{entries},
      program_code{std::move(program_code)}, program_code_b{std::move(program_code_b)} {
    if (!params.precompiled_variants) {
        return;
    }
    for (const auto& pair : *params.precompiled_variants) {
        auto locker = MakeLocker(system, program_type);
        const auto& usage = pair->first;
        FillLocker(*locker, usage);

        std::unique_ptr<LockerVariant>* locker_variant = nullptr;
        const auto it =
            std::find_if(locker_variants.begin(), locker_variants.end(), [&](const auto& variant) {
                return variant->locker->HasEqualKeys(*locker);
            });
        if (it == locker_variants.end()) {
            locker_variant = &locker_variants.emplace_back();
            *locker_variant = std::make_unique<LockerVariant>();
            locker_variant->get()->locker = std::move(locker);
        } else {
            locker_variant = &*it;
        }
        locker_variant->get()->programs.emplace(usage.variant, pair->second);
    }
}

Shader CachedShader::CreateStageFromMemory(const ShaderParameters& params,
                                           Maxwell::ShaderProgram program_type,
                                           ProgramCode program_code, ProgramCode program_code_b) {
    params.disk_cache.SaveRaw(ShaderDiskCacheRaw(
        params.unique_identifier, GetProgramType(program_type), program_code, program_code_b));

    ConstBufferLocker locker(GetEnginesShaderType(GetProgramType(program_type)));
    const ShaderIR ir(program_code, STAGE_MAIN_OFFSET, COMPILER_SETTINGS, locker);
    // TODO(Rodrigo): Handle VertexA shaders
    // std::optional<ShaderIR> ir_b;
    // if (!program_code_b.empty()) {
    //     ir_b.emplace(program_code_b, STAGE_MAIN_OFFSET);
    // }
    return std::shared_ptr<CachedShader>(
        new CachedShader(params, GetProgramType(program_type), GLShader::GetEntries(ir),
                         std::move(program_code), std::move(program_code_b)));
}

Shader CachedShader::CreateKernelFromMemory(const ShaderParameters& params, ProgramCode code) {
    params.disk_cache.SaveRaw(
        ShaderDiskCacheRaw(params.unique_identifier, ProgramType::Compute, code));

    ConstBufferLocker locker(Tegra::Engines::ShaderType::Compute);
    const ShaderIR ir(code, KERNEL_MAIN_OFFSET, COMPILER_SETTINGS, locker);
    return std::shared_ptr<CachedShader>(new CachedShader(
        params, ProgramType::Compute, GLShader::GetEntries(ir), std::move(code), {}));
}

Shader CachedShader::CreateFromCache(const ShaderParameters& params,
                                     const UnspecializedShader& unspecialized) {
    return std::shared_ptr<CachedShader>(new CachedShader(params, unspecialized.program_type,
                                                          unspecialized.entries, unspecialized.code,
                                                          unspecialized.code_b));
}

std::tuple<GLuint, BaseBindings> CachedShader::GetProgramHandle(const ProgramVariant& variant) {
    UpdateVariant();

    const auto [entry, is_cache_miss] = curr_variant->programs.try_emplace(variant);
    auto& program = entry->second;
    if (is_cache_miss) {
        program = BuildShader(device, unique_identifier, program_type, program_code, program_code_b,
                              variant, *curr_variant->locker);
        disk_cache.SaveUsage(GetUsage(variant, *curr_variant->locker));

        LabelGLObject(GL_PROGRAM, program->handle, cpu_addr);
    }

    auto base_bindings = variant.base_bindings;
    base_bindings.cbuf += static_cast<u32>(entries.const_buffers.size());
    if (program_type != ProgramType::Compute) {
        base_bindings.cbuf += STAGE_RESERVED_UBOS;
    }
    base_bindings.gmem += static_cast<u32>(entries.global_memory_entries.size());
    base_bindings.sampler += static_cast<u32>(entries.samplers.size());

    return {program->handle, base_bindings};
}

void CachedShader::UpdateVariant() {
    if (curr_variant && !curr_variant->locker->IsConsistent()) {
        curr_variant = nullptr;
    }
    if (!curr_variant) {
        for (auto& variant : locker_variants) {
            if (variant->locker->IsConsistent()) {
                curr_variant = variant.get();
            }
        }
    }
    if (!curr_variant) {
        auto& new_variant = locker_variants.emplace_back();
        new_variant = std::make_unique<LockerVariant>();
        new_variant->locker = MakeLocker(system, program_type);
        curr_variant = new_variant.get();
    }
}

ShaderDiskCacheUsage CachedShader::GetUsage(const ProgramVariant& variant,
                                            const ConstBufferLocker& locker) const {
    ShaderDiskCacheUsage usage;
    usage.unique_identifier = unique_identifier;
    usage.variant = variant;
    usage.keys = locker.GetKeys();
    usage.bound_samplers = locker.GetBoundSamplers();
    usage.bindless_samplers = locker.GetBindlessSamplers();
    return usage;
}

ShaderCacheOpenGL::ShaderCacheOpenGL(RasterizerOpenGL& rasterizer, Core::System& system,
                                     Core::Frontend::EmuWindow& emu_window, const Device& device)
    : RasterizerCache{rasterizer}, system{system}, emu_window{emu_window}, device{device},
      disk_cache{system} {}

void ShaderCacheOpenGL::LoadDiskCache(const std::atomic_bool& stop_loading,
                                      const VideoCore::DiskResourceLoadCallback& callback) {
    const auto transferable = disk_cache.LoadTransferable();
    if (!transferable) {
        return;
    }
    const auto [raws, shader_usages] = *transferable;
    if (!GenerateUnspecializedShaders(stop_loading, callback, raws) || stop_loading) {
        return;
    }

    const auto dumps = disk_cache.LoadPrecompiled();
    const auto supported_formats = GetSupportedFormats();

    // Track if precompiled cache was altered during loading to know if we have to
    // serialize the virtual precompiled cache file back to the hard drive
    bool precompiled_cache_altered = false;

    // Inform the frontend about shader build initialization
    if (callback) {
        callback(VideoCore::LoadCallbackStage::Build, 0, shader_usages.size());
    }

    std::mutex mutex;
    std::size_t built_shaders = 0; // It doesn't have be atomic since it's used behind a mutex
    std::atomic_bool compilation_failed = false;

    const auto Worker = [&](Core::Frontend::GraphicsContext* context, std::size_t begin,
                            std::size_t end, const std::vector<ShaderDiskCacheUsage>& shader_usages,
                            const ShaderDumpsMap& dumps) {
        context->MakeCurrent();
        SCOPE_EXIT({ return context->DoneCurrent(); });

        for (std::size_t i = begin; i < end; ++i) {
            if (stop_loading || compilation_failed) {
                return;
            }
            const auto& usage{shader_usages[i]};
            const auto& unspecialized{unspecialized_shaders.at(usage.unique_identifier)};
            const auto dump{dumps.find(usage)};

            CachedProgram shader;
            if (dump != dumps.end()) {
                // If the shader is dumped, attempt to load it with
                shader = GeneratePrecompiledProgram(dump->second, supported_formats);
                if (!shader) {
                    compilation_failed = true;
                    return;
                }
            }
            if (!shader) {
                auto locker{MakeLocker(system, unspecialized.program_type)};
                FillLocker(*locker, usage);
                shader = BuildShader(device, usage.unique_identifier, unspecialized.program_type,
                                     unspecialized.code, unspecialized.code_b, usage.variant,
                                     *locker, true);
            }

            std::scoped_lock lock{mutex};
            if (callback) {
                callback(VideoCore::LoadCallbackStage::Build, ++built_shaders,
                         shader_usages.size());
            }

            precompiled_programs.emplace(usage, std::move(shader));

            // TODO(Rodrigo): Is there a better way to do this?
            precompiled_variants[usage.unique_identifier].push_back(
                precompiled_programs.find(usage));
        }
    };

    const auto num_workers{static_cast<std::size_t>(std::thread::hardware_concurrency() + 1ULL)};
    const std::size_t bucket_size{shader_usages.size() / num_workers};
    std::vector<std::unique_ptr<Core::Frontend::GraphicsContext>> contexts(num_workers);
    std::vector<std::thread> threads(num_workers);
    for (std::size_t i = 0; i < num_workers; ++i) {
        const bool is_last_worker = i + 1 == num_workers;
        const std::size_t start{bucket_size * i};
        const std::size_t end{is_last_worker ? shader_usages.size() : start + bucket_size};

        // On some platforms the shared context has to be created from the GUI thread
        contexts[i] = emu_window.CreateSharedContext();
        threads[i] = std::thread(Worker, contexts[i].get(), start, end, shader_usages, dumps);
    }
    for (auto& thread : threads) {
        thread.join();
    }

    if (compilation_failed) {
        // Invalidate the precompiled cache if a shader dumped shader was rejected
        disk_cache.InvalidatePrecompiled();
        precompiled_cache_altered = true;
        return;
    }
    if (stop_loading) {
        return;
    }

    // TODO(Rodrigo): Do state tracking for transferable shaders and do a dummy draw
    // before precompiling them

    for (std::size_t i = 0; i < shader_usages.size(); ++i) {
        const auto& usage{shader_usages[i]};
        if (dumps.find(usage) == dumps.end()) {
            const auto& program{precompiled_programs.at(usage)};
            disk_cache.SaveDump(usage, program->handle);
            precompiled_cache_altered = true;
        }
    }

    if (precompiled_cache_altered) {
        disk_cache.SaveVirtualPrecompiledFile();
    }
}

const PrecompiledVariants* ShaderCacheOpenGL::GetPrecompiledVariants(u64 unique_identifier) const {
    const auto it = precompiled_variants.find(unique_identifier);
    return it == precompiled_variants.end() ? nullptr : &it->second;
}

CachedProgram ShaderCacheOpenGL::GeneratePrecompiledProgram(
    const ShaderDiskCacheDump& dump, const std::unordered_set<GLenum>& supported_formats) {
    if (supported_formats.find(dump.binary_format) == supported_formats.end()) {
        LOG_INFO(Render_OpenGL, "Precompiled cache entry with unsupported format - removing");
        return {};
    }

    CachedProgram shader = std::make_shared<OGLProgram>();
    shader->handle = glCreateProgram();
    glProgramParameteri(shader->handle, GL_PROGRAM_SEPARABLE, GL_TRUE);
    glProgramBinary(shader->handle, dump.binary_format, dump.binary.data(),
                    static_cast<GLsizei>(dump.binary.size()));

    GLint link_status{};
    glGetProgramiv(shader->handle, GL_LINK_STATUS, &link_status);
    if (link_status == GL_FALSE) {
        LOG_INFO(Render_OpenGL, "Precompiled cache rejected by the driver - removing");
        return {};
    }

    return shader;
}

bool ShaderCacheOpenGL::GenerateUnspecializedShaders(
    const std::atomic_bool& stop_loading, const VideoCore::DiskResourceLoadCallback& callback,
    const std::vector<ShaderDiskCacheRaw>& raws) {
    if (callback) {
        callback(VideoCore::LoadCallbackStage::Decompile, 0, raws.size());
    }

    for (std::size_t i = 0; i < raws.size(); ++i) {
        if (stop_loading) {
            return false;
        }
        const auto& raw{raws[i]};
        const u64 unique_identifier{raw.GetUniqueIdentifier()};
        const u64 calculated_hash{
            GetUniqueIdentifier(raw.GetProgramType(), raw.GetProgramCode(), raw.GetProgramCodeB())};
        if (unique_identifier != calculated_hash) {
            LOG_ERROR(Render_OpenGL,
                      "Invalid hash in entry={:016x} (obtained hash={:016x}) - "
                      "removing shader cache",
                      raw.GetUniqueIdentifier(), calculated_hash);
            disk_cache.InvalidateTransferable();
            return false;
        }

        const u32 main_offset =
            raw.GetProgramType() == ProgramType::Compute ? KERNEL_MAIN_OFFSET : STAGE_MAIN_OFFSET;
        ConstBufferLocker locker(GetEnginesShaderType(raw.GetProgramType()));
        const ShaderIR ir(raw.GetProgramCode(), main_offset, COMPILER_SETTINGS, locker);
        // TODO(Rodrigo): Handle VertexA shaders
        // std::optional<ShaderIR> ir_b;
        // if (raw.HasProgramA()) {
        //     ir_b.emplace(raw.GetProgramCodeB(), main_offset);
        // }

        UnspecializedShader unspecialized;
        unspecialized.entries = GLShader::GetEntries(ir);
        unspecialized.program_type = raw.GetProgramType();
        unspecialized.code = raw.GetProgramCode();
        unspecialized.code_b = raw.GetProgramCodeB();
        unspecialized_shaders.emplace(raw.GetUniqueIdentifier(), unspecialized);

        if (callback) {
            callback(VideoCore::LoadCallbackStage::Decompile, i, raws.size());
        }
    }
    return true;
}

Shader ShaderCacheOpenGL::GetStageProgram(Maxwell::ShaderProgram program) {
    if (!system.GPU().Maxwell3D().dirty.shaders) {
        return last_shaders[static_cast<std::size_t>(program)];
    }

    auto& memory_manager{system.GPU().MemoryManager()};
    const GPUVAddr address{GetShaderAddress(system, program)};

    // Look up shader in the cache based on address
    const auto host_ptr{memory_manager.GetPointer(address)};
    Shader shader{TryGet(host_ptr)};
    if (shader) {
        return last_shaders[static_cast<std::size_t>(program)] = shader;
    }

    // No shader found - create a new one
    ProgramCode code{GetShaderCode(memory_manager, address, host_ptr)};
    ProgramCode code_b;
    if (program == Maxwell::ShaderProgram::VertexA) {
        const GPUVAddr address_b{GetShaderAddress(system, Maxwell::ShaderProgram::VertexB)};
        code_b = GetShaderCode(memory_manager, address_b, memory_manager.GetPointer(address_b));
    }

    const auto unique_identifier = GetUniqueIdentifier(GetProgramType(program), code, code_b);
    const auto precompiled_variants = GetPrecompiledVariants(unique_identifier);
    const auto cpu_addr{*memory_manager.GpuToCpuAddress(address)};
    const ShaderParameters params{system,   disk_cache, precompiled_variants, device,
                                  cpu_addr, host_ptr,   unique_identifier};

    const auto found = unspecialized_shaders.find(unique_identifier);
    if (found == unspecialized_shaders.end()) {
        shader = CachedShader::CreateStageFromMemory(params, program, std::move(code),
                                                     std::move(code_b));
    } else {
        shader = CachedShader::CreateFromCache(params, found->second);
    }
    Register(shader);

    return last_shaders[static_cast<std::size_t>(program)] = shader;
}

Shader ShaderCacheOpenGL::GetComputeKernel(GPUVAddr code_addr) {
    auto& memory_manager{system.GPU().MemoryManager()};
    const auto host_ptr{memory_manager.GetPointer(code_addr)};
    auto kernel = TryGet(host_ptr);
    if (kernel) {
        return kernel;
    }

    // No kernel found - create a new one
    auto code{GetShaderCode(memory_manager, code_addr, host_ptr)};
    const auto unique_identifier{GetUniqueIdentifier(ProgramType::Compute, code, {})};
    const auto precompiled_variants = GetPrecompiledVariants(unique_identifier);
    const auto cpu_addr{*memory_manager.GpuToCpuAddress(code_addr)};
    const ShaderParameters params{system,   disk_cache, precompiled_variants, device,
                                  cpu_addr, host_ptr,   unique_identifier};

    const auto found = unspecialized_shaders.find(unique_identifier);
    if (found == unspecialized_shaders.end()) {
        kernel = CachedShader::CreateKernelFromMemory(params, std::move(code));
    } else {
        kernel = CachedShader::CreateFromCache(params, found->second);
    }

    Register(kernel);
    return kernel;
}

} // namespace OpenGL
