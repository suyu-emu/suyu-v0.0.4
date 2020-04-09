// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <atomic>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_set>

#include <boost/functional/hash.hpp>

#include "common/alignment.h"
#include "common/assert.h"
#include "common/logging/log.h"
#include "common/scope_exit.h"
#include "core/core.h"
#include "core/frontend/emu_window.h"
#include "video_core/engines/kepler_compute.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/engines/shader_type.h"
#include "video_core/memory_manager.h"
#include "video_core/renderer_opengl/gl_rasterizer.h"
#include "video_core/renderer_opengl/gl_shader_cache.h"
#include "video_core/renderer_opengl/gl_shader_decompiler.h"
#include "video_core/renderer_opengl/gl_shader_disk_cache.h"
#include "video_core/renderer_opengl/gl_state_tracker.h"
#include "video_core/renderer_opengl/utils.h"
#include "video_core/shader/registry.h"
#include "video_core/shader/shader_ir.h"

namespace OpenGL {

using Tegra::Engines::ShaderType;
using VideoCommon::Shader::ProgramCode;
using VideoCommon::Shader::Registry;
using VideoCommon::Shader::ShaderIR;

namespace {

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
std::size_t CalculateProgramSize(const ProgramCode& program) {
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
    ProgramCode code(VideoCommon::Shader::MAX_PROGRAM_LENGTH);
    ASSERT_OR_EXECUTE(host_ptr != nullptr, {
        std::fill(code.begin(), code.end(), 0);
        return code;
    });
    memory_manager.ReadBlockUnsafe(gpu_addr, code.data(), code.size() * sizeof(u64));
    code.resize(CalculateProgramSize(code));
    return code;
}

/// Gets the shader type from a Maxwell program type
constexpr GLenum GetGLShaderType(ShaderType shader_type) {
    switch (shader_type) {
    case ShaderType::Vertex:
        return GL_VERTEX_SHADER;
    case ShaderType::Geometry:
        return GL_GEOMETRY_SHADER;
    case ShaderType::Fragment:
        return GL_FRAGMENT_SHADER;
    case ShaderType::Compute:
        return GL_COMPUTE_SHADER;
    default:
        return GL_NONE;
    }
}

/// Hashes one (or two) program streams
u64 GetUniqueIdentifier(ShaderType shader_type, bool is_a, const ProgramCode& code,
                        const ProgramCode& code_b = {}) {
    u64 unique_identifier = boost::hash_value(code);
    if (is_a) {
        // VertexA programs include two programs
        boost::hash_combine(unique_identifier, boost::hash_value(code_b));
    }
    return unique_identifier;
}

constexpr const char* GetShaderTypeName(ShaderType shader_type) {
    switch (shader_type) {
    case ShaderType::Vertex:
        return "VS";
    case ShaderType::TesselationControl:
        return "HS";
    case ShaderType::TesselationEval:
        return "DS";
    case ShaderType::Geometry:
        return "GS";
    case ShaderType::Fragment:
        return "FS";
    case ShaderType::Compute:
        return "CS";
    }
    return "UNK";
}

constexpr ShaderType GetShaderType(Maxwell::ShaderProgram program_type) {
    switch (program_type) {
    case Maxwell::ShaderProgram::VertexA:
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
    }
    return {};
}

std::string MakeShaderID(u64 unique_identifier, ShaderType shader_type) {
    return fmt::format("{}{:016X}", GetShaderTypeName(shader_type), unique_identifier);
}

std::shared_ptr<Registry> MakeRegistry(const ShaderDiskCacheEntry& entry) {
    const VideoCore::GuestDriverProfile guest_profile{entry.texture_handler_size};
    const VideoCommon::Shader::SerializedRegistryInfo info{guest_profile, entry.bound_buffer,
                                                           entry.graphics_info, entry.compute_info};
    const auto registry = std::make_shared<Registry>(entry.type, info);
    for (const auto& [address, value] : entry.keys) {
        const auto [buffer, offset] = address;
        registry->InsertKey(buffer, offset, value);
    }
    for (const auto& [offset, sampler] : entry.bound_samplers) {
        registry->InsertBoundSampler(offset, sampler);
    }
    for (const auto& [key, sampler] : entry.bindless_samplers) {
        const auto [buffer, offset] = key;
        registry->InsertBindlessSampler(buffer, offset, sampler);
    }
    return registry;
}

std::shared_ptr<OGLProgram> BuildShader(const Device& device, ShaderType shader_type,
                                        u64 unique_identifier, const ShaderIR& ir,
                                        const Registry& registry, bool hint_retrievable = false) {
    const std::string shader_id = MakeShaderID(unique_identifier, shader_type);
    LOG_INFO(Render_OpenGL, "{}", shader_id);

    const std::string glsl = DecompileShader(device, ir, registry, shader_type, shader_id);
    OGLShader shader;
    shader.Create(glsl.c_str(), GetGLShaderType(shader_type));

    auto program = std::make_shared<OGLProgram>();
    program->Create(true, hint_retrievable, shader.handle);
    return program;
}

std::unordered_set<GLenum> GetSupportedFormats() {
    GLint num_formats;
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

CachedShader::CachedShader(VAddr cpu_addr, std::size_t size_in_bytes,
                           std::shared_ptr<VideoCommon::Shader::Registry> registry,
                           ShaderEntries entries, std::shared_ptr<OGLProgram> program)
    : RasterizerCacheObject{cpu_addr}, registry{std::move(registry)}, entries{std::move(entries)},
      size_in_bytes{size_in_bytes}, program{std::move(program)} {}

CachedShader::~CachedShader() = default;

GLuint CachedShader::GetHandle() const {
    DEBUG_ASSERT(registry->IsConsistent());
    return program->handle;
}

Shader CachedShader::CreateStageFromMemory(const ShaderParameters& params,
                                           Maxwell::ShaderProgram program_type, ProgramCode code,
                                           ProgramCode code_b) {
    const auto shader_type = GetShaderType(program_type);
    const std::size_t size_in_bytes = code.size() * sizeof(u64);

    auto registry = std::make_shared<Registry>(shader_type, params.system.GPU().Maxwell3D());
    const ShaderIR ir(code, STAGE_MAIN_OFFSET, COMPILER_SETTINGS, *registry);
    // TODO(Rodrigo): Handle VertexA shaders
    // std::optional<ShaderIR> ir_b;
    // if (!code_b.empty()) {
    //     ir_b.emplace(code_b, STAGE_MAIN_OFFSET);
    // }
    auto program = BuildShader(params.device, shader_type, params.unique_identifier, ir, *registry);

    ShaderDiskCacheEntry entry;
    entry.type = shader_type;
    entry.code = std::move(code);
    entry.code_b = std::move(code_b);
    entry.unique_identifier = params.unique_identifier;
    entry.bound_buffer = registry->GetBoundBuffer();
    entry.graphics_info = registry->GetGraphicsInfo();
    entry.keys = registry->GetKeys();
    entry.bound_samplers = registry->GetBoundSamplers();
    entry.bindless_samplers = registry->GetBindlessSamplers();
    params.disk_cache.SaveEntry(std::move(entry));

    return std::shared_ptr<CachedShader>(new CachedShader(
        params.cpu_addr, size_in_bytes, std::move(registry), MakeEntries(ir), std::move(program)));
}

Shader CachedShader::CreateKernelFromMemory(const ShaderParameters& params, ProgramCode code) {
    const std::size_t size_in_bytes = code.size() * sizeof(u64);

    auto& engine = params.system.GPU().KeplerCompute();
    auto registry = std::make_shared<Registry>(ShaderType::Compute, engine);
    const ShaderIR ir(code, KERNEL_MAIN_OFFSET, COMPILER_SETTINGS, *registry);
    const u64 uid = params.unique_identifier;
    auto program = BuildShader(params.device, ShaderType::Compute, uid, ir, *registry);

    ShaderDiskCacheEntry entry;
    entry.type = ShaderType::Compute;
    entry.code = std::move(code);
    entry.unique_identifier = uid;
    entry.bound_buffer = registry->GetBoundBuffer();
    entry.compute_info = registry->GetComputeInfo();
    entry.keys = registry->GetKeys();
    entry.bound_samplers = registry->GetBoundSamplers();
    entry.bindless_samplers = registry->GetBindlessSamplers();
    params.disk_cache.SaveEntry(std::move(entry));

    return std::shared_ptr<CachedShader>(new CachedShader(
        params.cpu_addr, size_in_bytes, std::move(registry), MakeEntries(ir), std::move(program)));
}

Shader CachedShader::CreateFromCache(const ShaderParameters& params,
                                     const PrecompiledShader& precompiled_shader,
                                     std::size_t size_in_bytes) {
    return std::shared_ptr<CachedShader>(
        new CachedShader(params.cpu_addr, size_in_bytes, precompiled_shader.registry,
                         precompiled_shader.entries, precompiled_shader.program));
}

ShaderCacheOpenGL::ShaderCacheOpenGL(RasterizerOpenGL& rasterizer, Core::System& system,
                                     Core::Frontend::EmuWindow& emu_window, const Device& device)
    : RasterizerCache{rasterizer}, system{system}, emu_window{emu_window}, device{device},
      disk_cache{system} {}

void ShaderCacheOpenGL::LoadDiskCache(const std::atomic_bool& stop_loading,
                                      const VideoCore::DiskResourceLoadCallback& callback) {
    const std::optional transferable = disk_cache.LoadTransferable();
    if (!transferable) {
        return;
    }

    const std::vector gl_cache = disk_cache.LoadPrecompiled();
    const auto supported_formats = GetSupportedFormats();

    // Track if precompiled cache was altered during loading to know if we have to
    // serialize the virtual precompiled cache file back to the hard drive
    bool precompiled_cache_altered = false;

    // Inform the frontend about shader build initialization
    if (callback) {
        callback(VideoCore::LoadCallbackStage::Build, 0, transferable->size());
    }

    std::mutex mutex;
    std::size_t built_shaders = 0; // It doesn't have be atomic since it's used behind a mutex
    std::atomic_bool gl_cache_failed = false;

    const auto find_precompiled = [&gl_cache](u64 id) {
        return std::find_if(gl_cache.begin(), gl_cache.end(),
                            [id](const auto& entry) { return entry.unique_identifier == id; });
    };

    const auto worker = [&](Core::Frontend::GraphicsContext* context, std::size_t begin,
                            std::size_t end) {
        const auto scope = context->Acquire();

        for (std::size_t i = begin; i < end; ++i) {
            if (stop_loading) {
                return;
            }
            const auto& entry = (*transferable)[i];
            const u64 uid = entry.unique_identifier;
            const auto it = find_precompiled(uid);
            const auto precompiled_entry = it != gl_cache.end() ? &*it : nullptr;

            const bool is_compute = entry.type == ShaderType::Compute;
            const u32 main_offset = is_compute ? KERNEL_MAIN_OFFSET : STAGE_MAIN_OFFSET;
            auto registry = MakeRegistry(entry);
            const ShaderIR ir(entry.code, main_offset, COMPILER_SETTINGS, *registry);

            std::shared_ptr<OGLProgram> program;
            if (precompiled_entry) {
                // If the shader is precompiled, attempt to load it with
                program = GeneratePrecompiledProgram(entry, *precompiled_entry, supported_formats);
                if (!program) {
                    gl_cache_failed = true;
                }
            }
            if (!program) {
                // Otherwise compile it from GLSL
                program = BuildShader(device, entry.type, uid, ir, *registry, true);
            }

            PrecompiledShader shader;
            shader.program = std::move(program);
            shader.registry = std::move(registry);
            shader.entries = MakeEntries(ir);

            std::scoped_lock lock{mutex};
            if (callback) {
                callback(VideoCore::LoadCallbackStage::Build, ++built_shaders,
                         transferable->size());
            }
            runtime_cache.emplace(entry.unique_identifier, std::move(shader));
        }
    };

    const auto num_workers{static_cast<std::size_t>(std::thread::hardware_concurrency() + 1ULL)};
    const std::size_t bucket_size{transferable->size() / num_workers};
    std::vector<std::unique_ptr<Core::Frontend::GraphicsContext>> contexts(num_workers);
    std::vector<std::thread> threads(num_workers);
    for (std::size_t i = 0; i < num_workers; ++i) {
        const bool is_last_worker = i + 1 == num_workers;
        const std::size_t start{bucket_size * i};
        const std::size_t end{is_last_worker ? transferable->size() : start + bucket_size};

        // On some platforms the shared context has to be created from the GUI thread
        contexts[i] = emu_window.CreateSharedContext();
        threads[i] = std::thread(worker, contexts[i].get(), start, end);
    }
    for (auto& thread : threads) {
        thread.join();
    }

    if (gl_cache_failed) {
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

    for (std::size_t i = 0; i < transferable->size(); ++i) {
        const u64 id = (*transferable)[i].unique_identifier;
        const auto it = find_precompiled(id);
        if (it == gl_cache.end()) {
            const GLuint program = runtime_cache.at(id).program->handle;
            disk_cache.SavePrecompiled(id, program);
            precompiled_cache_altered = true;
        }
    }

    if (precompiled_cache_altered) {
        disk_cache.SaveVirtualPrecompiledFile();
    }
}

std::shared_ptr<OGLProgram> ShaderCacheOpenGL::GeneratePrecompiledProgram(
    const ShaderDiskCacheEntry& entry, const ShaderDiskCachePrecompiled& precompiled_entry,
    const std::unordered_set<GLenum>& supported_formats) {
    if (supported_formats.find(precompiled_entry.binary_format) == supported_formats.end()) {
        LOG_INFO(Render_OpenGL, "Precompiled cache entry with unsupported format, removing");
        return {};
    }

    auto program = std::make_shared<OGLProgram>();
    program->handle = glCreateProgram();
    glProgramParameteri(program->handle, GL_PROGRAM_SEPARABLE, GL_TRUE);
    glProgramBinary(program->handle, precompiled_entry.binary_format,
                    precompiled_entry.binary.data(),
                    static_cast<GLsizei>(precompiled_entry.binary.size()));

    GLint link_status;
    glGetProgramiv(program->handle, GL_LINK_STATUS, &link_status);
    if (link_status == GL_FALSE) {
        LOG_INFO(Render_OpenGL, "Precompiled cache rejected by the driver, removing");
        return {};
    }

    return program;
}

Shader ShaderCacheOpenGL::GetStageProgram(Maxwell::ShaderProgram program) {
    if (!system.GPU().Maxwell3D().dirty.flags[Dirty::Shaders]) {
        return last_shaders[static_cast<std::size_t>(program)];
    }

    auto& memory_manager{system.GPU().MemoryManager()};
    const GPUVAddr address{GetShaderAddress(system, program)};

    // Look up shader in the cache based on address
    const auto cpu_addr{memory_manager.GpuToCpuAddress(address)};
    Shader shader{cpu_addr ? TryGet(*cpu_addr) : nullptr};
    if (shader) {
        return last_shaders[static_cast<std::size_t>(program)] = shader;
    }

    const auto host_ptr{memory_manager.GetPointer(address)};

    // No shader found - create a new one
    ProgramCode code{GetShaderCode(memory_manager, address, host_ptr)};
    ProgramCode code_b;
    if (program == Maxwell::ShaderProgram::VertexA) {
        const GPUVAddr address_b{GetShaderAddress(system, Maxwell::ShaderProgram::VertexB)};
        code_b = GetShaderCode(memory_manager, address_b, memory_manager.GetPointer(address_b));
    }

    const auto unique_identifier = GetUniqueIdentifier(
        GetShaderType(program), program == Maxwell::ShaderProgram::VertexA, code, code_b);

    const ShaderParameters params{system,    disk_cache, device,
                                  *cpu_addr, host_ptr,   unique_identifier};

    const auto found = runtime_cache.find(unique_identifier);
    if (found == runtime_cache.end()) {
        shader = CachedShader::CreateStageFromMemory(params, program, std::move(code),
                                                     std::move(code_b));
    } else {
        const std::size_t size_in_bytes = code.size() * sizeof(u64);
        shader = CachedShader::CreateFromCache(params, found->second, size_in_bytes);
    }
    Register(shader);

    return last_shaders[static_cast<std::size_t>(program)] = shader;
}

Shader ShaderCacheOpenGL::GetComputeKernel(GPUVAddr code_addr) {
    auto& memory_manager{system.GPU().MemoryManager()};
    const auto cpu_addr{memory_manager.GpuToCpuAddress(code_addr)};

    auto kernel = cpu_addr ? TryGet(*cpu_addr) : nullptr;
    if (kernel) {
        return kernel;
    }

    const auto host_ptr{memory_manager.GetPointer(code_addr)};
    // No kernel found, create a new one
    auto code{GetShaderCode(memory_manager, code_addr, host_ptr)};
    const auto unique_identifier{GetUniqueIdentifier(ShaderType::Compute, false, code)};

    const ShaderParameters params{system,    disk_cache, device,
                                  *cpu_addr, host_ptr,   unique_identifier};

    const auto found = runtime_cache.find(unique_identifier);
    if (found == runtime_cache.end()) {
        kernel = CachedShader::CreateKernelFromMemory(params, std::move(code));
    } else {
        const std::size_t size_in_bytes = code.size() * sizeof(u64);
        kernel = CachedShader::CreateFromCache(params, found->second, size_in_bytes);
    }

    Register(kernel);
    return kernel;
}

} // namespace OpenGL
