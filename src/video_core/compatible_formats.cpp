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
    PixelFormat::R32G32B32A32_FLOAT,
    PixelFormat::R32G32B32A32_UINT,
    PixelFormat::R32G32B32A32_SINT,
};

constexpr std::array VIEW_CLASS_96_BITS = {
    PixelFormat::R32G32B32_FLOAT,
};
// Missing formats:
// PixelFormat::RGB32UI,
// PixelFormat::RGB32I,

constexpr std::array VIEW_CLASS_64_BITS = {
    PixelFormat::R32G32_FLOAT,       PixelFormat::R32G32_UINT,
    PixelFormat::R32G32_SINT,        PixelFormat::R16G16B16A16_FLOAT,
    PixelFormat::R16G16B16A16_UNORM, PixelFormat::R16G16B16A16_SNORM,
    PixelFormat::R16G16B16A16_UINT,  PixelFormat::R16G16B16A16_SINT,
};

// TODO: How should we handle 48 bits?

constexpr std::array VIEW_CLASS_32_BITS = {
    PixelFormat::R16G16_FLOAT,      PixelFormat::B10G11R11_FLOAT, PixelFormat::R32_FLOAT,
    PixelFormat::A2B10G10R10_UNORM, PixelFormat::R16G16_UINT,     PixelFormat::R32_UINT,
    PixelFormat::R16G16_SINT,       PixelFormat::R32_SINT,        PixelFormat::A8B8G8R8_UNORM,
    PixelFormat::R16G16_UNORM,      PixelFormat::A8B8G8R8_SNORM,  PixelFormat::R16G16_SNORM,
    PixelFormat::A8B8G8R8_SRGB,     PixelFormat::E5B9G9R9_FLOAT,  PixelFormat::B8G8R8A8_UNORM,
    PixelFormat::B8G8R8A8_SRGB,     PixelFormat::A8B8G8R8_UINT,   PixelFormat::A8B8G8R8_SINT,
    PixelFormat::A2B10G10R10_UINT,
};

// TODO: How should we handle 24 bits?

constexpr std::array VIEW_CLASS_16_BITS = {
    PixelFormat::R16_FLOAT,  PixelFormat::R8G8_UINT,  PixelFormat::R16_UINT,
    PixelFormat::R16_SINT,   PixelFormat::R8G8_UNORM, PixelFormat::R16_UNORM,
    PixelFormat::R8G8_SNORM, PixelFormat::R16_SNORM,  PixelFormat::R8G8_SINT,
};

constexpr std::array VIEW_CLASS_8_BITS = {
    PixelFormat::R8_UINT,
    PixelFormat::R8_UNORM,
    PixelFormat::R8_SINT,
    PixelFormat::R8_SNORM,
};

constexpr std::array VIEW_CLASS_RGTC1_RED = {
    PixelFormat::BC4_UNORM,
    PixelFormat::BC4_SNORM,
};

constexpr std::array VIEW_CLASS_RGTC2_RG = {
    PixelFormat::BC5_UNORM,
    PixelFormat::BC5_SNORM,
};

constexpr std::array VIEW_CLASS_BPTC_UNORM = {
    PixelFormat::BC7_UNORM,
    PixelFormat::BC7_SRGB,
};

constexpr std::array VIEW_CLASS_BPTC_FLOAT = {
    PixelFormat::BC6H_SFLOAT,
    PixelFormat::BC6H_UFLOAT,
};

// Compatibility table taken from Table 4.X.1 in:
// https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_copy_image.txt

constexpr std::array COPY_CLASS_128_BITS = {
    PixelFormat::R32G32B32A32_UINT, PixelFormat::R32G32B32A32_FLOAT, PixelFormat::R32G32B32A32_SINT,
    PixelFormat::BC2_UNORM,         PixelFormat::BC2_SRGB,           PixelFormat::BC3_UNORM,
    PixelFormat::BC3_SRGB,          PixelFormat::BC5_UNORM,          PixelFormat::BC5_SNORM,
    PixelFormat::BC7_UNORM,         PixelFormat::BC7_SRGB,           PixelFormat::BC6H_SFLOAT,
    PixelFormat::BC6H_UFLOAT,
};
// Missing formats:
// PixelFormat::RGBA32I
// COMPRESSED_RG_RGTC2

constexpr std::array COPY_CLASS_64_BITS = {
    PixelFormat::R16G16B16A16_FLOAT, PixelFormat::R16G16B16A16_UINT,
    PixelFormat::R16G16B16A16_UNORM, PixelFormat::R16G16B16A16_SNORM,
    PixelFormat::R16G16B16A16_SINT,  PixelFormat::R32G32_UINT,
    PixelFormat::R32G32_FLOAT,       PixelFormat::R32G32_SINT,
    PixelFormat::BC1_RGBA_UNORM,     PixelFormat::BC1_RGBA_SRGB,
};
// Missing formats:
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
