// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <utility>
#include <vector>

#include <fmt/format.h>

#include "shader_recompiler/backend/msl/var_alloc.h"
#include "shader_recompiler/stage.h"

namespace Shader {
struct Info;
struct Profile;
struct RuntimeInfo;
} // namespace Shader

namespace Shader::Backend {
struct Bindings;
}

namespace Shader::IR {
class Inst;
struct Program;
} // namespace Shader::IR

namespace Shader::Backend::MSL {

struct GenericElementInfo {
    std::string name;
    u32 first_element{};
    u32 num_components{};
};

struct TextureImageDefinition {
    u32 binding;
    u32 count;
};

class EmitContext {
public:
    explicit EmitContext(IR::Program& program, Bindings& bindings, const Profile& profile_,
                         const RuntimeInfo& runtime_info_);

    template <MslVarType type, typename... Args>
    void Add(const char* format_str, IR::Inst& inst, Args&&... args) {
        const auto var_def{var_alloc.AddDefine(inst, type)};
        if (var_def.empty()) {
            // skip assignment.
            code += fmt::format(fmt::runtime(format_str + 3), std::forward<Args>(args)...);
        } else {
            code += fmt::format(fmt::runtime(format_str), var_def, std::forward<Args>(args)...);
        }
        // TODO: Remove this
        code += '\n';
    }

    template <typename... Args>
    void AddU1(const char* format_str, IR::Inst& inst, Args&&... args) {
        Add<MslVarType::U1>(format_str, inst, args...);
    }

    template <typename... Args>
    void AddF16x2(const char* format_str, IR::Inst& inst, Args&&... args) {
        Add<MslVarType::F16x2>(format_str, inst, args...);
    }

    template <typename... Args>
    void AddU32(const char* format_str, IR::Inst& inst, Args&&... args) {
        Add<MslVarType::U32>(format_str, inst, args...);
    }

    template <typename... Args>
    void AddF32(const char* format_str, IR::Inst& inst, Args&&... args) {
        Add<MslVarType::F32>(format_str, inst, args...);
    }

    template <typename... Args>
    void AddU64(const char* format_str, IR::Inst& inst, Args&&... args) {
        Add<MslVarType::U64>(format_str, inst, args...);
    }

    template <typename... Args>
    void AddF64(const char* format_str, IR::Inst& inst, Args&&... args) {
        Add<MslVarType::F64>(format_str, inst, args...);
    }

    template <typename... Args>
    void AddU32x2(const char* format_str, IR::Inst& inst, Args&&... args) {
        Add<MslVarType::U32x2>(format_str, inst, args...);
    }

    template <typename... Args>
    void AddF32x2(const char* format_str, IR::Inst& inst, Args&&... args) {
        Add<MslVarType::F32x2>(format_str, inst, args...);
    }

    template <typename... Args>
    void AddU32x3(const char* format_str, IR::Inst& inst, Args&&... args) {
        Add<MslVarType::U32x3>(format_str, inst, args...);
    }

    template <typename... Args>
    void AddF32x3(const char* format_str, IR::Inst& inst, Args&&... args) {
        Add<MslVarType::F32x3>(format_str, inst, args...);
    }

    template <typename... Args>
    void AddU32x4(const char* format_str, IR::Inst& inst, Args&&... args) {
        Add<MslVarType::U32x4>(format_str, inst, args...);
    }

    template <typename... Args>
    void AddF32x4(const char* format_str, IR::Inst& inst, Args&&... args) {
        Add<MslVarType::F32x4>(format_str, inst, args...);
    }

    template <typename... Args>
    void AddPrecF32(const char* format_str, IR::Inst& inst, Args&&... args) {
        Add<MslVarType::PrecF32>(format_str, inst, args...);
    }

    template <typename... Args>
    void AddPrecF64(const char* format_str, IR::Inst& inst, Args&&... args) {
        Add<MslVarType::PrecF64>(format_str, inst, args...);
    }

    template <typename... Args>
    void Add(const char* format_str, Args&&... args) {
        code += fmt::format(fmt::runtime(format_str), std::forward<Args>(args)...);
        // TODO: Remove this
        code += '\n';
    }

    std::string header;
    std::string code;
    VarAlloc var_alloc;
    const Info& info;
    const Profile& profile;
    const RuntimeInfo& runtime_info;

    Stage stage{};
    std::string_view stage_name = "invalid";
    std::string_view position_name = "gl_Position";

    std::vector<TextureImageDefinition> texture_buffers;
    std::vector<TextureImageDefinition> image_buffers;
    std::vector<TextureImageDefinition> textures;
    std::vector<TextureImageDefinition> images;
    std::array<std::array<GenericElementInfo, 4>, 32> output_generics{};

    u32 num_safety_loop_vars{};

    bool uses_y_direction{};
    bool uses_cc_carry{};
    bool uses_geometry_passthrough{};

private:
    void SetupExtensions();
    void DefineConstantBuffers(Bindings& bindings);
    void DefineConstantBufferIndirect();
    void DefineStorageBuffers(Bindings& bindings);
    void DefineGenericOutput(size_t index, u32 invocations);
    void DefineHelperFunctions();
    void DefineConstants();
    std::string DefineGlobalMemoryFunctions();
    void SetupImages(Bindings& bindings);
    void SetupTextures(Bindings& bindings);
};

} // namespace Shader::Backend::MSL
