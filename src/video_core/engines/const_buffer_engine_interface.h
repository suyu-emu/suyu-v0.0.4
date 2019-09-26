// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <type_traits>
#include "common/bit_field.h"
#include "common/common_types.h"
#include "video_core/engines/shader_bytecode.h"
#include "video_core/textures/texture.h"

namespace Tegra::Engines {

enum class ShaderType : u32 {
    Vertex = 0,
    TesselationControl = 1,
    TesselationEval = 2,
    Geometry = 3,
    Fragment = 4,
    Compute = 5,
};

struct SamplerDescriptor {
    union {
        BitField<0, 20, Tegra::Shader::TextureType> texture_type;
        BitField<20, 1, u32> is_array;
        BitField<21, 1, u32> is_buffer;
        BitField<22, 1, u32> is_shadow;
        u32 raw{};
    };

    bool operator==(const SamplerDescriptor& rhs) const noexcept {
        return raw == rhs.raw;
    }

    static SamplerDescriptor FromTicTexture(Tegra::Texture::TextureType tic_texture_type) {
        SamplerDescriptor result;
        switch (tic_texture_type) {
        case Tegra::Texture::TextureType::Texture1D:
            result.texture_type.Assign(Tegra::Shader::TextureType::Texture1D);
            result.is_array.Assign(0);
            result.is_buffer.Assign(0);
            result.is_shadow.Assign(0);
            return result;
        case Tegra::Texture::TextureType::Texture2D:
            result.texture_type.Assign(Tegra::Shader::TextureType::Texture2D);
            result.is_array.Assign(0);
            result.is_buffer.Assign(0);
            result.is_shadow.Assign(0);
            return result;
        case Tegra::Texture::TextureType::Texture3D:
            result.texture_type.Assign(Tegra::Shader::TextureType::Texture3D);
            result.is_array.Assign(0);
            result.is_buffer.Assign(0);
            result.is_shadow.Assign(0);
            return result;
        case Tegra::Texture::TextureType::TextureCubemap:
            result.texture_type.Assign(Tegra::Shader::TextureType::TextureCube);
            result.is_array.Assign(0);
            result.is_buffer.Assign(0);
            result.is_shadow.Assign(0);
            return result;
        case Tegra::Texture::TextureType::Texture1DArray:
            result.texture_type.Assign(Tegra::Shader::TextureType::Texture1D);
            result.is_array.Assign(1);
            result.is_buffer.Assign(0);
            result.is_shadow.Assign(0);
            return result;
        case Tegra::Texture::TextureType::Texture2DArray:
            result.texture_type.Assign(Tegra::Shader::TextureType::Texture2D);
            result.is_array.Assign(1);
            result.is_buffer.Assign(0);
            result.is_shadow.Assign(0);
            return result;
        case Tegra::Texture::TextureType::Texture1DBuffer: {
            result.texture_type.Assign(Tegra::Shader::TextureType::Texture1D);
            result.is_array.Assign(0);
            result.is_buffer.Assign(1);
            result.is_shadow.Assign(0);
            return result;
        }
        case Tegra::Texture::TextureType::Texture2DNoMipmap:
            result.texture_type.Assign(Tegra::Shader::TextureType::Texture2D);
            result.is_array.Assign(0);
            result.is_buffer.Assign(0);
            result.is_shadow.Assign(0);
            return result;
        case Tegra::Texture::TextureType::TextureCubeArray:
            result.texture_type.Assign(Tegra::Shader::TextureType::TextureCube);
            result.is_array.Assign(1);
            result.is_buffer.Assign(0);
            result.is_shadow.Assign(0);
            return result;
        default:
            result.texture_type.Assign(Tegra::Shader::TextureType::Texture2D);
            result.is_array.Assign(0);
            result.is_buffer.Assign(0);
            result.is_shadow.Assign(0);
            return result;
        }
    }
};
static_assert(std::is_trivially_copyable_v<SamplerDescriptor>);

class ConstBufferEngineInterface {
public:
    virtual ~ConstBufferEngineInterface() {}
    virtual u32 AccessConstBuffer32(ShaderType stage, u64 const_buffer, u64 offset) const = 0;
    virtual SamplerDescriptor AccessBoundSampler(ShaderType stage, u64 offset) const = 0;
    virtual SamplerDescriptor AccessBindlessSampler(ShaderType stage, u64 const_buffer,
                                                    u64 offset) const = 0;
    virtual u32 GetBoundBuffer() const = 0;
};

} // namespace Tegra::Engines
