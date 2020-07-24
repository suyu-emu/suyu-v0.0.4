// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include "common/common_types.h"
#include "common/logging/log.h"
#include "video_core/texture_cache/format_lookup_table.h"

namespace VideoCommon {

using Tegra::Texture::ComponentType;
using Tegra::Texture::TextureFormat;
using VideoCore::Surface::PixelFormat;

namespace {

constexpr auto SNORM = ComponentType::SNORM;
constexpr auto UNORM = ComponentType::UNORM;
constexpr auto SINT = ComponentType::SINT;
constexpr auto UINT = ComponentType::UINT;
constexpr auto FLOAT = ComponentType::FLOAT;
constexpr bool C = false; // Normal color
constexpr bool S = true;  // Srgb

struct Table {
    constexpr Table(TextureFormat texture_format, bool is_srgb, ComponentType red_component,
                    ComponentType green_component, ComponentType blue_component,
                    ComponentType alpha_component, PixelFormat pixel_format)
        : texture_format{texture_format}, pixel_format{pixel_format}, red_component{red_component},
          green_component{green_component}, blue_component{blue_component},
          alpha_component{alpha_component}, is_srgb{is_srgb} {}

    TextureFormat texture_format;
    PixelFormat pixel_format;
    ComponentType red_component;
    ComponentType green_component;
    ComponentType blue_component;
    ComponentType alpha_component;
    bool is_srgb;
};
constexpr std::array<Table, 86> DefinitionTable = {{
    {TextureFormat::A8R8G8B8, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::A8B8G8R8_UNORM},
    {TextureFormat::A8R8G8B8, C, SNORM, SNORM, SNORM, SNORM, PixelFormat::A8B8G8R8_SNORM},
    {TextureFormat::A8R8G8B8, C, UINT, UINT, UINT, UINT, PixelFormat::A8B8G8R8_UINT},
    {TextureFormat::A8R8G8B8, C, SINT, SINT, SINT, SINT, PixelFormat::A8B8G8R8_SINT},
    {TextureFormat::A8R8G8B8, S, UNORM, UNORM, UNORM, UNORM, PixelFormat::A8B8G8R8_SRGB},

    {TextureFormat::B5G6R5, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::B5G6R5_UNORM},

    {TextureFormat::A2B10G10R10, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::A2B10G10R10_UNORM},
    {TextureFormat::A2B10G10R10, C, UINT, UINT, UINT, UINT, PixelFormat::A2B10G10R10_UINT},

    {TextureFormat::A1B5G5R5, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::A1B5G5R5_UNORM},

    {TextureFormat::A4B4G4R4, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::A4B4G4R4_UNORM},

    {TextureFormat::R8, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::R8_UNORM},
    {TextureFormat::R8, C, SNORM, SNORM, SNORM, SNORM, PixelFormat::R8_SNORM},
    {TextureFormat::R8, C, UINT, UINT, UINT, UINT, PixelFormat::R8_UINT},
    {TextureFormat::R8, C, SINT, SINT, SINT, SINT, PixelFormat::R8_SINT},

    {TextureFormat::R8G8, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::R8G8_UNORM},
    {TextureFormat::R8G8, C, SNORM, SNORM, SNORM, SNORM, PixelFormat::R8G8_SNORM},
    {TextureFormat::R8G8, C, UINT, UINT, UINT, UINT, PixelFormat::R8G8_UINT},
    {TextureFormat::R8G8, C, SINT, SINT, SINT, SINT, PixelFormat::R8G8_SINT},

    {TextureFormat::R16G16B16A16, C, SNORM, SNORM, SNORM, SNORM, PixelFormat::R16G16B16A16_SNORM},
    {TextureFormat::R16G16B16A16, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::R16G16B16A16_UNORM},
    {TextureFormat::R16G16B16A16, C, FLOAT, FLOAT, FLOAT, FLOAT, PixelFormat::R16G16B16A16_FLOAT},
    {TextureFormat::R16G16B16A16, C, UINT, UINT, UINT, UINT, PixelFormat::R16G16B16A16_UINT},
    {TextureFormat::R16G16B16A16, C, SINT, SINT, SINT, SINT, PixelFormat::R16G16B16A16_SINT},

    {TextureFormat::R16G16, C, FLOAT, FLOAT, FLOAT, FLOAT, PixelFormat::R16G16_FLOAT},
    {TextureFormat::R16G16, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::R16G16_UNORM},
    {TextureFormat::R16G16, C, SNORM, SNORM, SNORM, SNORM, PixelFormat::R16G16_SNORM},
    {TextureFormat::R16G16, C, UINT, UINT, UINT, UINT, PixelFormat::R16G16_UINT},
    {TextureFormat::R16G16, C, SINT, SINT, SINT, SINT, PixelFormat::R16G16_SINT},

    {TextureFormat::R16, C, FLOAT, FLOAT, FLOAT, FLOAT, PixelFormat::R16_FLOAT},
    {TextureFormat::R16, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::R16_UNORM},
    {TextureFormat::R16, C, SNORM, SNORM, SNORM, SNORM, PixelFormat::R16_SNORM},
    {TextureFormat::R16, C, UINT, UINT, UINT, UINT, PixelFormat::R16_UINT},
    {TextureFormat::R16, C, SINT, SINT, SINT, SINT, PixelFormat::R16_SINT},

    {TextureFormat::B10G11R11, C, FLOAT, FLOAT, FLOAT, FLOAT, PixelFormat::B10G11R11_FLOAT},

    {TextureFormat::R32G32B32A32, C, FLOAT, FLOAT, FLOAT, FLOAT, PixelFormat::R32G32B32A32_FLOAT},
    {TextureFormat::R32G32B32A32, C, UINT, UINT, UINT, UINT, PixelFormat::R32G32B32A32_UINT},
    {TextureFormat::R32G32B32A32, C, SINT, SINT, SINT, SINT, PixelFormat::R32G32B32A32_SINT},

    {TextureFormat::R32G32B32, C, FLOAT, FLOAT, FLOAT, FLOAT, PixelFormat::R32G32B32_FLOAT},

    {TextureFormat::R32G32, C, FLOAT, FLOAT, FLOAT, FLOAT, PixelFormat::R32G32_FLOAT},
    {TextureFormat::R32G32, C, UINT, UINT, UINT, UINT, PixelFormat::R32G32_UINT},
    {TextureFormat::R32G32, C, SINT, SINT, SINT, SINT, PixelFormat::R32G32_SINT},

    {TextureFormat::R32, C, FLOAT, FLOAT, FLOAT, FLOAT, PixelFormat::R32_FLOAT},
    {TextureFormat::R32, C, UINT, UINT, UINT, UINT, PixelFormat::R32_UINT},
    {TextureFormat::R32, C, SINT, SINT, SINT, SINT, PixelFormat::R32_SINT},

    {TextureFormat::E5B9G9R9, C, FLOAT, FLOAT, FLOAT, FLOAT, PixelFormat::E5B9G9R9_FLOAT},

    {TextureFormat::D32, C, FLOAT, FLOAT, FLOAT, FLOAT, PixelFormat::D32_FLOAT},
    {TextureFormat::D16, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::D16_UNORM},
    {TextureFormat::S8D24, C, UINT, UNORM, UNORM, UNORM, PixelFormat::S8_UINT_D24_UNORM},
    {TextureFormat::R8G24, C, UINT, UNORM, UNORM, UNORM, PixelFormat::S8_UINT_D24_UNORM},
    {TextureFormat::D32S8, C, FLOAT, UINT, UNORM, UNORM, PixelFormat::D32_FLOAT_S8_UINT},

    {TextureFormat::BC1_RGBA, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::BC1_RGBA_UNORM},
    {TextureFormat::BC1_RGBA, S, UNORM, UNORM, UNORM, UNORM, PixelFormat::BC1_RGBA_SRGB},

    {TextureFormat::BC2, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::BC2_UNORM},
    {TextureFormat::BC2, S, UNORM, UNORM, UNORM, UNORM, PixelFormat::BC2_SRGB},

    {TextureFormat::BC3, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::BC3_UNORM},
    {TextureFormat::BC3, S, UNORM, UNORM, UNORM, UNORM, PixelFormat::BC3_SRGB},

    {TextureFormat::BC4, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::BC4_UNORM},
    {TextureFormat::BC4, C, SNORM, SNORM, SNORM, SNORM, PixelFormat::BC4_SNORM},

    {TextureFormat::BC5, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::BC5_UNORM},
    {TextureFormat::BC5, C, SNORM, SNORM, SNORM, SNORM, PixelFormat::BC5_SNORM},

    {TextureFormat::BC7, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::BC7_UNORM},
    {TextureFormat::BC7, S, UNORM, UNORM, UNORM, UNORM, PixelFormat::BC7_SRGB},

    {TextureFormat::BC6H_SFLOAT, C, FLOAT, FLOAT, FLOAT, FLOAT, PixelFormat::BC6H_SFLOAT},
    {TextureFormat::BC6H_UFLOAT, C, FLOAT, FLOAT, FLOAT, FLOAT, PixelFormat::BC6H_UFLOAT},

    {TextureFormat::ASTC_2D_4X4, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::ASTC_2D_4X4_UNORM},
    {TextureFormat::ASTC_2D_4X4, S, UNORM, UNORM, UNORM, UNORM, PixelFormat::ASTC_2D_4X4_SRGB},

    {TextureFormat::ASTC_2D_5X4, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::ASTC_2D_5X4_UNORM},
    {TextureFormat::ASTC_2D_5X4, S, UNORM, UNORM, UNORM, UNORM, PixelFormat::ASTC_2D_5X4_SRGB},

    {TextureFormat::ASTC_2D_5X5, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::ASTC_2D_5X5_UNORM},
    {TextureFormat::ASTC_2D_5X5, S, UNORM, UNORM, UNORM, UNORM, PixelFormat::ASTC_2D_5X5_SRGB},

    {TextureFormat::ASTC_2D_8X8, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::ASTC_2D_8X8_UNORM},
    {TextureFormat::ASTC_2D_8X8, S, UNORM, UNORM, UNORM, UNORM, PixelFormat::ASTC_2D_8X8_SRGB},

    {TextureFormat::ASTC_2D_8X5, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::ASTC_2D_8X5_UNORM},
    {TextureFormat::ASTC_2D_8X5, S, UNORM, UNORM, UNORM, UNORM, PixelFormat::ASTC_2D_8X5_SRGB},

    {TextureFormat::ASTC_2D_10X8, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::ASTC_2D_10X8_UNORM},
    {TextureFormat::ASTC_2D_10X8, S, UNORM, UNORM, UNORM, UNORM, PixelFormat::ASTC_2D_10X8_SRGB},

    {TextureFormat::ASTC_2D_6X6, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::ASTC_2D_6X6_UNORM},
    {TextureFormat::ASTC_2D_6X6, S, UNORM, UNORM, UNORM, UNORM, PixelFormat::ASTC_2D_6X6_SRGB},

    {TextureFormat::ASTC_2D_10X10, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::ASTC_2D_10X10_UNORM},
    {TextureFormat::ASTC_2D_10X10, S, UNORM, UNORM, UNORM, UNORM, PixelFormat::ASTC_2D_10X10_SRGB},

    {TextureFormat::ASTC_2D_12X12, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::ASTC_2D_12X12_UNORM},
    {TextureFormat::ASTC_2D_12X12, S, UNORM, UNORM, UNORM, UNORM, PixelFormat::ASTC_2D_12X12_SRGB},

    {TextureFormat::ASTC_2D_8X6, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::ASTC_2D_8X6_UNORM},
    {TextureFormat::ASTC_2D_8X6, S, UNORM, UNORM, UNORM, UNORM, PixelFormat::ASTC_2D_8X6_SRGB},

    {TextureFormat::ASTC_2D_6X5, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::ASTC_2D_6X5_UNORM},
    {TextureFormat::ASTC_2D_6X5, S, UNORM, UNORM, UNORM, UNORM, PixelFormat::ASTC_2D_6X5_SRGB},
}};

} // Anonymous namespace

FormatLookupTable::FormatLookupTable() {
    table.fill(static_cast<u8>(PixelFormat::Invalid));

    for (const auto& entry : DefinitionTable) {
        table[CalculateIndex(entry.texture_format, entry.is_srgb != 0, entry.red_component,
                             entry.green_component, entry.blue_component, entry.alpha_component)] =
            static_cast<u8>(entry.pixel_format);
    }
}

PixelFormat FormatLookupTable::GetPixelFormat(TextureFormat format, bool is_srgb,
                                              ComponentType red_component,
                                              ComponentType green_component,
                                              ComponentType blue_component,
                                              ComponentType alpha_component) const noexcept {
    const auto pixel_format = static_cast<PixelFormat>(table[CalculateIndex(
        format, is_srgb, red_component, green_component, blue_component, alpha_component)]);
    // [[likely]]
    if (pixel_format != PixelFormat::Invalid) {
        return pixel_format;
    }
    UNIMPLEMENTED_MSG("texture format={} srgb={} components={{{} {} {} {}}}",
                      static_cast<int>(format), is_srgb, static_cast<int>(red_component),
                      static_cast<int>(green_component), static_cast<int>(blue_component),
                      static_cast<int>(alpha_component));
    return PixelFormat::A8B8G8R8_UNORM;
}

void FormatLookupTable::Set(TextureFormat format, bool is_srgb, ComponentType red_component,
                            ComponentType green_component, ComponentType blue_component,
                            ComponentType alpha_component, PixelFormat pixel_format) {}

std::size_t FormatLookupTable::CalculateIndex(TextureFormat format, bool is_srgb,
                                              ComponentType red_component,
                                              ComponentType green_component,
                                              ComponentType blue_component,
                                              ComponentType alpha_component) noexcept {
    const auto format_index = static_cast<std::size_t>(format);
    const auto red_index = static_cast<std::size_t>(red_component);
    const auto green_index = static_cast<std::size_t>(green_component);
    const auto blue_index = static_cast<std::size_t>(blue_component);
    const auto alpha_index = static_cast<std::size_t>(alpha_component);
    const std::size_t srgb_index = is_srgb ? 1 : 0;

    return format_index * PerFormat +
           srgb_index * PerComponent * PerComponent * PerComponent * PerComponent +
           alpha_index * PerComponent * PerComponent * PerComponent +
           blue_index * PerComponent * PerComponent + green_index * PerComponent + red_index;
}

} // namespace VideoCommon
