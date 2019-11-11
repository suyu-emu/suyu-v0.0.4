// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <limits>
#include "video_core/surface.h"
#include "video_core/textures/texture.h"

namespace VideoCommon {

class FormatLookupTable {
public:
    explicit FormatLookupTable();

    VideoCore::Surface::PixelFormat GetPixelFormat(
        Tegra::Texture::TextureFormat format, bool is_srgb,
        Tegra::Texture::ComponentType red_component, Tegra::Texture::ComponentType green_component,
        Tegra::Texture::ComponentType blue_component,
        Tegra::Texture::ComponentType alpha_component) const noexcept;

private:
    static_assert(VideoCore::Surface::MaxPixelFormat <= std::numeric_limits<u8>::max());

    static constexpr std::size_t NumTextureFormats = 128;

    static constexpr std::size_t PerComponent = 8;
    static constexpr std::size_t PerComponents2 = PerComponent * PerComponent;
    static constexpr std::size_t PerComponents3 = PerComponents2 * PerComponent;
    static constexpr std::size_t PerComponents4 = PerComponents3 * PerComponent;
    static constexpr std::size_t PerFormat = PerComponents4 * 2;

    static std::size_t CalculateIndex(Tegra::Texture::TextureFormat format, bool is_srgb,
                                      Tegra::Texture::ComponentType red_component,
                                      Tegra::Texture::ComponentType green_component,
                                      Tegra::Texture::ComponentType blue_component,
                                      Tegra::Texture::ComponentType alpha_component) noexcept;

    void Set(Tegra::Texture::TextureFormat format, bool is_srgb,
             Tegra::Texture::ComponentType red_component,
             Tegra::Texture::ComponentType green_component,
             Tegra::Texture::ComponentType blue_component,
             Tegra::Texture::ComponentType alpha_component,
             VideoCore::Surface::PixelFormat pixel_format);

    std::array<u8, NumTextureFormats * PerFormat> table;
};

} // namespace VideoCommon
