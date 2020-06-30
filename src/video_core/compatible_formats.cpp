// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <bitset>
#include <cstddef>

#include "video_core/compatible_formats.h"
#include "video_core/surface.h"

namespace VideoCore::Surface {

namespace {

// Compatibility table taken from Table 3.X.2 in:
// https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_texture_view.txt

constexpr std::array VIEW_CLASS_128_BITS = {
    PixelFormat::RGBA32F,
    PixelFormat::RGBA32UI,
};
// Missing formats:
// PixelFormat::RGBA32I

constexpr std::array VIEW_CLASS_96_BITS = {
    PixelFormat::RGB32F,
};
// Missing formats:
// PixelFormat::RGB32UI,
// PixelFormat::RGB32I,

constexpr std::array VIEW_CLASS_64_BITS = {
    PixelFormat::RGBA16F, PixelFormat::RG32F,   PixelFormat::RGBA16UI, PixelFormat::RG32UI,
    PixelFormat::RGBA16U, PixelFormat::RGBA16F, PixelFormat::RGBA16S,
};
// Missing formats:
// PixelFormat::RGBA16I
// PixelFormat::RG32I

// TODO: How should we handle 48 bits?

constexpr std::array VIEW_CLASS_32_BITS = {
    PixelFormat::RG16F,        PixelFormat::R11FG11FB10F, PixelFormat::R32F,
    PixelFormat::A2B10G10R10U, PixelFormat::RG16UI,       PixelFormat::R32UI,
    PixelFormat::RG16I,        PixelFormat::R32I,         PixelFormat::ABGR8U,
    PixelFormat::RG16,         PixelFormat::ABGR8S,       PixelFormat::RG16S,
    PixelFormat::RGBA8_SRGB,   PixelFormat::E5B9G9R9F,    PixelFormat::BGRA8,
    PixelFormat::BGRA8_SRGB,
};
// Missing formats:
// PixelFormat::RGBA8UI
// PixelFormat::RGBA8I
// PixelFormat::RGB10_A2_UI

// TODO: How should we handle 24 bits?

constexpr std::array VIEW_CLASS_16_BITS = {
    PixelFormat::R16F, PixelFormat::RG8UI, PixelFormat::R16UI, PixelFormat::R16I,
    PixelFormat::RG8U, PixelFormat::R16U,  PixelFormat::RG8S,  PixelFormat::R16S,
};
// Missing formats:
// PixelFormat::RG8I

constexpr std::array VIEW_CLASS_8_BITS = {
    PixelFormat::R8UI,
    PixelFormat::R8U,
};
// Missing formats:
// PixelFormat::R8I
// PixelFormat::R8S

constexpr std::array VIEW_CLASS_RGTC1_RED = {
    PixelFormat::DXN1,
};
// Missing formats:
// COMPRESSED_SIGNED_RED_RGTC1

constexpr std::array VIEW_CLASS_RGTC2_RG = {
    PixelFormat::DXN2UNORM,
    PixelFormat::DXN2SNORM,
};

constexpr std::array VIEW_CLASS_BPTC_UNORM = {
    PixelFormat::BC7U,
    PixelFormat::BC7U_SRGB,
};

constexpr std::array VIEW_CLASS_BPTC_FLOAT = {
    PixelFormat::BC6H_SF16,
    PixelFormat::BC6H_UF16,
};

// Compatibility table taken from Table 4.X.1 in:
// https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_copy_image.txt

constexpr std::array COPY_CLASS_128_BITS = {
    PixelFormat::RGBA32UI,   PixelFormat::RGBA32F,   PixelFormat::DXT23,
    PixelFormat::DXT23_SRGB, PixelFormat::DXT45,     PixelFormat::DXT45_SRGB,
    PixelFormat::DXN2SNORM,  PixelFormat::BC7U,      PixelFormat::BC7U_SRGB,
    PixelFormat::BC6H_SF16,  PixelFormat::BC6H_UF16,
};
// Missing formats:
// PixelFormat::RGBA32I
// COMPRESSED_RG_RGTC2

constexpr std::array COPY_CLASS_64_BITS = {
    PixelFormat::RGBA16F, PixelFormat::RG32F,   PixelFormat::RGBA16UI,  PixelFormat::RG32UI,
    PixelFormat::RGBA16U, PixelFormat::RGBA16S, PixelFormat::DXT1_SRGB, PixelFormat::DXT1,

};
// Missing formats:
// PixelFormat::RGBA16I
// PixelFormat::RG32I,
// COMPRESSED_RGB_S3TC_DXT1_EXT
// COMPRESSED_SRGB_S3TC_DXT1_EXT
// COMPRESSED_RGBA_S3TC_DXT1_EXT
// COMPRESSED_SIGNED_RED_RGTC1

void Enable(FormatCompatibility::Table& compatiblity, size_t format_a, size_t format_b) {
    compatiblity[format_a][format_b] = true;
    compatiblity[format_b][format_a] = true;
}

void Enable(FormatCompatibility::Table& compatibility, PixelFormat format_a, PixelFormat format_b) {
    Enable(compatibility, static_cast<size_t>(format_a), static_cast<size_t>(format_b));
}

template <typename Range>
void EnableRange(FormatCompatibility::Table& compatibility, const Range& range) {
    for (auto it_a = range.begin(); it_a != range.end(); ++it_a) {
        for (auto it_b = it_a; it_b != range.end(); ++it_b) {
            Enable(compatibility, *it_a, *it_b);
        }
    }
}

} // Anonymous namespace

FormatCompatibility::FormatCompatibility() {
    for (size_t i = 0; i < MaxPixelFormat; ++i) {
        // Identity is allowed
        Enable(view, i, i);
    }

    EnableRange(view, VIEW_CLASS_128_BITS);
    EnableRange(view, VIEW_CLASS_96_BITS);
    EnableRange(view, VIEW_CLASS_64_BITS);
    EnableRange(view, VIEW_CLASS_32_BITS);
    EnableRange(view, VIEW_CLASS_16_BITS);
    EnableRange(view, VIEW_CLASS_8_BITS);
    EnableRange(view, VIEW_CLASS_RGTC1_RED);
    EnableRange(view, VIEW_CLASS_RGTC2_RG);
    EnableRange(view, VIEW_CLASS_BPTC_UNORM);
    EnableRange(view, VIEW_CLASS_BPTC_FLOAT);

    copy = view;
    EnableRange(copy, COPY_CLASS_128_BITS);
    EnableRange(copy, COPY_CLASS_64_BITS);
}

} // namespace VideoCore::Surface
