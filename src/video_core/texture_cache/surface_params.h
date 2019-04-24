// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <map>

#include "common/common_types.h"
#include "video_core/engines/fermi_2d.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/surface.h"

namespace VideoCommon {

class HasheableSurfaceParams {
public:
    std::size_t Hash() const;

    bool operator==(const HasheableSurfaceParams& rhs) const;

    bool operator!=(const HasheableSurfaceParams& rhs) const {
        return !operator==(rhs);
    }

protected:
    // Avoid creation outside of a managed environment.
    HasheableSurfaceParams() = default;

    bool is_tiled;
    bool srgb_conversion;
    u32 block_width;
    u32 block_height;
    u32 block_depth;
    u32 tile_width_spacing;
    u32 width;
    u32 height;
    u32 depth;
    u32 pitch;
    u32 unaligned_height;
    u32 num_levels;
    VideoCore::Surface::PixelFormat pixel_format;
    VideoCore::Surface::ComponentType component_type;
    VideoCore::Surface::SurfaceType type;
    VideoCore::Surface::SurfaceTarget target;
};

class SurfaceParams final : public HasheableSurfaceParams {
public:
    /// Creates SurfaceCachedParams from a texture configuration.
    static SurfaceParams CreateForTexture(Core::System& system,
                                          const Tegra::Texture::FullTextureInfo& config);

    /// Creates SurfaceCachedParams for a depth buffer configuration.
    static SurfaceParams CreateForDepthBuffer(
        Core::System& system, u32 zeta_width, u32 zeta_height, Tegra::DepthFormat format,
        u32 block_width, u32 block_height, u32 block_depth,
        Tegra::Engines::Maxwell3D::Regs::InvMemoryLayout type);

    /// Creates SurfaceCachedParams from a framebuffer configuration.
    static SurfaceParams CreateForFramebuffer(Core::System& system, std::size_t index);

    /// Creates SurfaceCachedParams from a Fermi2D surface configuration.
    static SurfaceParams CreateForFermiCopySurface(
        const Tegra::Engines::Fermi2D::Regs::Surface& config);

    bool IsTiled() const {
        return is_tiled;
    }

    bool GetSrgbConversion() const {
        return srgb_conversion;
    }

    u32 GetBlockWidth() const {
        return block_width;
    }

    u32 GetTileWidthSpacing() const {
        return tile_width_spacing;
    }

    u32 GetWidth() const {
        return width;
    }

    u32 GetHeight() const {
        return height;
    }

    u32 GetDepth() const {
        return depth;
    }

    u32 GetPitch() const {
        return pitch;
    }

    u32 GetNumLevels() const {
        return num_levels;
    }

    VideoCore::Surface::PixelFormat GetPixelFormat() const {
        return pixel_format;
    }

    VideoCore::Surface::ComponentType GetComponentType() const {
        return component_type;
    }

    VideoCore::Surface::SurfaceTarget GetTarget() const {
        return target;
    }

    VideoCore::Surface::SurfaceType GetType() const {
        return type;
    }

    std::size_t GetGuestSizeInBytes() const {
        return guest_size_in_bytes;
    }

    std::size_t GetHostSizeInBytes() const {
        return host_size_in_bytes;
    }

    u32 GetNumLayers() const {
        return num_layers;
    }

    /// Returns the width of a given mipmap level.
    u32 GetMipWidth(u32 level) const;

    /// Returns the height of a given mipmap level.
    u32 GetMipHeight(u32 level) const;

    /// Returns the depth of a given mipmap level.
    u32 GetMipDepth(u32 level) const;

    /// Returns true if these parameters are from a layered surface.
    bool IsLayered() const;

    /// Returns the block height of a given mipmap level.
    u32 GetMipBlockHeight(u32 level) const;

    /// Returns the block depth of a given mipmap level.
    u32 GetMipBlockDepth(u32 level) const;

    /// Returns the offset in bytes in guest memory of a given mipmap level.
    std::size_t GetGuestMipmapLevelOffset(u32 level) const;

    /// Returns the offset in bytes in host memory (linear) of a given mipmap level.
    std::size_t GetHostMipmapLevelOffset(u32 level) const;

    /// Returns the size in bytes in host memory (linear) of a given mipmap level.
    std::size_t GetHostMipmapSize(u32 level) const;

    /// Returns the size of a layer in bytes in guest memory.
    std::size_t GetGuestLayerSize() const;

    /// Returns the size of a layer in bytes in host memory for a given mipmap level.
    std::size_t GetHostLayerSize(u32 level) const;

    /// Returns the default block width.
    u32 GetDefaultBlockWidth() const;

    /// Returns the default block height.
    u32 GetDefaultBlockHeight() const;

    /// Returns the bits per pixel.
    u32 GetBitsPerPixel() const;

    /// Returns the bytes per pixel.
    u32 GetBytesPerPixel() const;

    /// Returns true if another surface can be familiar with this. This is a loosely defined term
    /// that reflects the possibility of these two surface parameters potentially being part of a
    /// bigger superset.
    bool IsFamiliar(const SurfaceParams& view_params) const;

    /// Returns true if the pixel format is a depth and/or stencil format.
    bool IsPixelFormatZeta() const;

    /// Creates a map that redirects an address difference to a layer and mipmap level.
    std::map<u64, std::pair<u32, u32>> CreateViewOffsetMap() const;

    /// Returns true if the passed surface view parameters is equal or a valid subset of this.
    bool IsViewValid(const SurfaceParams& view_params, u32 layer, u32 level) const;

private:
    /// Calculates values that can be deduced from HasheableSurfaceParams.
    void CalculateCachedValues();

    /// Returns the size of a given mipmap level inside a layer.
    std::size_t GetInnerMipmapMemorySize(u32 level, bool as_host_size, bool uncompressed) const;

    /// Returns the size of all mipmap levels and aligns as needed.
    std::size_t GetInnerMemorySize(bool as_host_size, bool layer_only, bool uncompressed) const;

    /// Returns the size of a layer
    std::size_t GetLayerSize(bool as_host_size, bool uncompressed) const;

    /// Returns true if the passed view width and height match the size of this params in a given
    /// mipmap level.
    bool IsDimensionValid(const SurfaceParams& view_params, u32 level) const;

    /// Returns true if the passed view depth match the size of this params in a given mipmap level.
    bool IsDepthValid(const SurfaceParams& view_params, u32 level) const;

    /// Returns true if the passed view layers and mipmap levels are in bounds.
    bool IsInBounds(const SurfaceParams& view_params, u32 layer, u32 level) const;

    std::size_t guest_size_in_bytes;
    std::size_t host_size_in_bytes;
    u32 num_layers;
};

} // namespace VideoCommon

namespace std {

template <>
struct hash<VideoCommon::SurfaceParams> {
    std::size_t operator()(const VideoCommon::SurfaceParams& k) const noexcept {
        return k.Hash();
    }
};

} // namespace std
