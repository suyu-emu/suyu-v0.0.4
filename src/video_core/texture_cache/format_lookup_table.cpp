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
constexpr auto SNORM_FORCE_FP16 = ComponentType::SNORM_FORCE_FP16;
constexpr auto UNORM_FORCE_FP16 = ComponentType::UNORM_FORCE_FP16;
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
constexpr std::array<Table, 76> DefinitionTable = {{
    {TextureFormat::A8R8G8B8, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::ABGR8U},
    {TextureFormat::A8R8G8B8, C, SNORM, SNORM, SNORM, SNORM, PixelFormat::ABGR8S},
    {TextureFormat::A8R8G8B8, C, UINT, UINT, UINT, UINT, PixelFormat::ABGR8UI},
    {TextureFormat::A8R8G8B8, S, UNORM, UNORM, UNORM, UNORM, PixelFormat::RGBA8_SRGB},

    {TextureFormat::B5G6R5, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::B5G6R5U},

    {TextureFormat::A2B10G10R10, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::A2B10G10R10U},

    {TextureFormat::A1B5G5R5, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::A1B5G5R5U},

    {TextureFormat::A4B4G4R4, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::R4G4B4A4U},

    {TextureFormat::R8, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::R8U},
    {TextureFormat::R8, C, UINT, UINT, UINT, UINT, PixelFormat::R8UI},

    {TextureFormat::G8R8, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::RG8U},
    {TextureFormat::G8R8, C, SNORM, SNORM, SNORM, SNORM, PixelFormat::RG8S},

    {TextureFormat::R16_G16_B16_A16, C, SNORM, SNORM, SNORM, SNORM, PixelFormat::RGBA16S},
    {TextureFormat::R16_G16_B16_A16, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::RGBA16U},
    {TextureFormat::R16_G16_B16_A16, C, FLOAT, FLOAT, FLOAT, FLOAT, PixelFormat::RGBA16F},
    {TextureFormat::R16_G16_B16_A16, C, UINT, UINT, UINT, UINT, PixelFormat::RGBA16UI},

    {TextureFormat::R16_G16, C, FLOAT, FLOAT, FLOAT, FLOAT, PixelFormat::RG16F},
    {TextureFormat::R16_G16, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::RG16},
    {TextureFormat::R16_G16, C, SNORM, SNORM, SNORM, SNORM, PixelFormat::RG16S},
    {TextureFormat::R16_G16, C, UINT, UINT, UINT, UINT, PixelFormat::RG16UI},
    {TextureFormat::R16_G16, C, SINT, SINT, SINT, SINT, PixelFormat::RG16I},

    {TextureFormat::R16, C, FLOAT, FLOAT, FLOAT, FLOAT, PixelFormat::R16F},
    {TextureFormat::R16, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::R16U},
    {TextureFormat::R16, C, SNORM, SNORM, SNORM, SNORM, PixelFormat::R16S},
    {TextureFormat::R16, C, UINT, UINT, UINT, UINT, PixelFormat::R16UI},
    {TextureFormat::R16, C, SINT, SINT, SINT, SINT, PixelFormat::R16I},

    {TextureFormat::BF10GF11RF11, C, FLOAT, FLOAT, FLOAT, FLOAT, PixelFormat::R11FG11FB10F},

    {TextureFormat::R32_G32_B32_A32, C, FLOAT, FLOAT, FLOAT, FLOAT, PixelFormat::RGBA32F},
    {TextureFormat::R32_G32_B32_A32, C, UINT, UINT, UINT, UINT, PixelFormat::RGBA32UI},

    {TextureFormat::R32_G32_B32, C, FLOAT, FLOAT, FLOAT, FLOAT, PixelFormat::RGB32F},

    {TextureFormat::R32_G32, C, FLOAT, FLOAT, FLOAT, FLOAT, PixelFormat::RG32F},
    {TextureFormat::R32_G32, C, UINT, UINT, UINT, UINT, PixelFormat::RG32UI},

    {TextureFormat::R32, C, FLOAT, FLOAT, FLOAT, FLOAT, PixelFormat::R32F},
    {TextureFormat::R32, C, UINT, UINT, UINT, UINT, PixelFormat::R32UI},
    {TextureFormat::R32, C, SINT, SINT, SINT, SINT, PixelFormat::R32I},

    {TextureFormat::E5B9G9R9_SHAREDEXP, C, FLOAT, FLOAT, FLOAT, FLOAT, PixelFormat::E5B9G9R9F},

    {TextureFormat::ZF32, C, FLOAT, FLOAT, FLOAT, FLOAT, PixelFormat::Z32F},
    {TextureFormat::Z16, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::Z16},
    {TextureFormat::S8Z24, C, UINT, UNORM, UNORM, UNORM, PixelFormat::S8Z24},
    {TextureFormat::ZF32_X24S8, C, FLOAT, UINT, UNORM, UNORM, PixelFormat::Z32FS8},

    {TextureFormat::DXT1, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::DXT1},
    {TextureFormat::DXT1, S, UNORM, UNORM, UNORM, UNORM, PixelFormat::DXT1_SRGB},

    {TextureFormat::DXT23, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::DXT23},
    {TextureFormat::DXT23, S, UNORM, UNORM, UNORM, UNORM, PixelFormat::DXT23_SRGB},

    {TextureFormat::DXT45, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::DXT45},
    {TextureFormat::DXT45, S, UNORM, UNORM, UNORM, UNORM, PixelFormat::DXT45_SRGB},

    // TODO: Use a different pixel format for SNORM
    {TextureFormat::DXN1, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::DXN1},
    {TextureFormat::DXN1, C, SNORM, SNORM, SNORM, SNORM, PixelFormat::DXN1},

    {TextureFormat::DXN2, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::DXN2UNORM},
    {TextureFormat::DXN2, C, SNORM, SNORM, SNORM, SNORM, PixelFormat::DXN2SNORM},

    {TextureFormat::BC7U, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::BC7U},
    {TextureFormat::BC7U, S, UNORM, UNORM, UNORM, UNORM, PixelFormat::BC7U_SRGB},

    {TextureFormat::BC6H_SF16, C, FLOAT, FLOAT, FLOAT, FLOAT, PixelFormat::BC6H_SF16},
    {TextureFormat::BC6H_UF16, C, FLOAT, FLOAT, FLOAT, FLOAT, PixelFormat::BC6H_UF16},

    {TextureFormat::ASTC_2D_4X4, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::ASTC_2D_4X4},
    {TextureFormat::ASTC_2D_4X4, S, UNORM, UNORM, UNORM, UNORM, PixelFormat::ASTC_2D_4X4_SRGB},

    {TextureFormat::ASTC_2D_5X4, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::ASTC_2D_5X4},
    {TextureFormat::ASTC_2D_5X4, S, UNORM, UNORM, UNORM, UNORM, PixelFormat::ASTC_2D_5X4_SRGB},

    {TextureFormat::ASTC_2D_5X5, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::ASTC_2D_5X5},
    {TextureFormat::ASTC_2D_5X5, S, UNORM, UNORM, UNORM, UNORM, PixelFormat::ASTC_2D_5X5_SRGB},

    {TextureFormat::ASTC_2D_8X8, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::ASTC_2D_8X8},
    {TextureFormat::ASTC_2D_8X8, S, UNORM, UNORM, UNORM, UNORM, PixelFormat::ASTC_2D_8X8_SRGB},

    {TextureFormat::ASTC_2D_8X5, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::ASTC_2D_8X5},
    {TextureFormat::ASTC_2D_8X5, S, UNORM, UNORM, UNORM, UNORM, PixelFormat::ASTC_2D_8X5_SRGB},

    {TextureFormat::ASTC_2D_10X8, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::ASTC_2D_10X8},
    {TextureFormat::ASTC_2D_10X8, S, UNORM, UNORM, UNORM, UNORM, PixelFormat::ASTC_2D_10X8_SRGB},

    {TextureFormat::ASTC_2D_6X6, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::ASTC_2D_6X6},
    {TextureFormat::ASTC_2D_6X6, S, UNORM, UNORM, UNORM, UNORM, PixelFormat::ASTC_2D_6X6_SRGB},

    {TextureFormat::ASTC_2D_10X10, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::ASTC_2D_10X10},
    {TextureFormat::ASTC_2D_10X10, S, UNORM, UNORM, UNORM, UNORM, PixelFormat::ASTC_2D_10X10_SRGB},

    {TextureFormat::ASTC_2D_12X12, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::ASTC_2D_12X12},
    {TextureFormat::ASTC_2D_12X12, S, UNORM, UNORM, UNORM, UNORM, PixelFormat::ASTC_2D_12X12_SRGB},

    {TextureFormat::ASTC_2D_8X6, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::ASTC_2D_8X6},
    {TextureFormat::ASTC_2D_8X6, S, UNORM, UNORM, UNORM, UNORM, PixelFormat::ASTC_2D_8X6_SRGB},

    {TextureFormat::ASTC_2D_6X5, C, UNORM, UNORM, UNORM, UNORM, PixelFormat::ASTC_2D_6X5},
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
    return PixelFormat::ABGR8U;
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
