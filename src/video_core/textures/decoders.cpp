// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cmath>
#include <cstring>
#include "common/assert.h"
#include "core/memory.h"
#include "video_core/gpu.h"
#include "video_core/textures/decoders.h"
#include "video_core/textures/texture.h"

namespace Tegra::Texture {

/**
 * Calculates the offset of an (x, y) position within a swizzled texture.
 * Taken from the Tegra X1 Technical Reference Manual. pages 1187-1188
 */
static u32 GetSwizzleOffset(u32 x, u32 y, u32 bytes_per_pixel, u32 gob_address) {
    // Round up to the next gob
    x *= bytes_per_pixel;
    u32 address = gob_address + ((x % 64) / 32) * 256 + ((y % 8) / 2) * 64 + ((x % 32) / 16) * 32 +
                  (y % 2) * 16 + (x % 16);

    return address;
}

void CopySwizzledData(u32 width, u32 height, u32 bytes_per_pixel, u32 out_bytes_per_pixel,
                      u8* swizzled_data, u8* unswizzled_data, bool unswizzle, u32 block_height) {
    std::array<u8*, 2> data_ptrs;
    const u32 stride = width * bytes_per_pixel;
    const u32 gobs_in_x = 64;
    const u32 gobs_in_y = 8;
    const u32 gobs_size = gobs_in_x * gobs_in_y;
    const u32 image_width_in_gobs{(stride + gobs_in_x - 1) / gobs_in_x};
    for (unsigned y = 0; y < height; ++y) {
        const u32 gob_y_address =
            (y / (gobs_in_y * block_height)) * gobs_size * block_height * image_width_in_gobs +
            (y % (gobs_in_y * block_height) / gobs_in_y) * gobs_size;
        for (unsigned x = 0; x < width; ++x) {
            const u32 gob_address =
                gob_y_address + (x * bytes_per_pixel / gobs_in_x) * gobs_size * block_height;
            const u32 swizzle_offset = GetSwizzleOffset(x, y, bytes_per_pixel, gob_address);
            const u32 pixel_index = (x + y * width) * out_bytes_per_pixel;

            data_ptrs[unswizzle] = swizzled_data + swizzle_offset;
            data_ptrs[!unswizzle] = unswizzled_data + pixel_index;

            std::memcpy(data_ptrs[0], data_ptrs[1], bytes_per_pixel);
        }
    }
}

// This table represents the internal swizzle of a gob.
template <std::size_t N, std::size_t M>
struct alignas(64) SwizzleTable {
    constexpr SwizzleTable() {
        for (u32 y = 0; y < N; ++y) {
            for (u32 x = 0; x < M; ++x) {
                const u32 x2 = x * 16;
                values[y][x] = static_cast<u16>(((x2 % 64) / 32) * 256 + ((y % 8) / 2) * 64 +
                                                ((x2 % 32) / 16) * 32 + (y % 2) * 16);
            }
        }
    }
    const std::array<u16, M>& operator[](std::size_t index) const {
        return values[index];
    }
    std::array<std::array<u16, M>, N> values{};
};

constexpr auto swizzle_table = SwizzleTable<8, 4>();

void FastSwizzleData(u32 width, u32 height, u32 bytes_per_pixel, u8* swizzled_data,
                     u8* unswizzled_data, bool unswizzle, u32 block_height) {
    std::array<u8*, 2> data_ptrs;
    const std::size_t stride{width * bytes_per_pixel};
    const std::size_t gobs_in_x = 64;
    const std::size_t gobs_in_y = 8;
    const std::size_t gobs_size = gobs_in_x * gobs_in_y;
    const std::size_t image_width_in_gobs{(stride + gobs_in_x - 1) / gobs_in_x};
    const std::size_t copy_size{16};
    for (std::size_t y = 0; y < height; ++y) {
        const std::size_t initial_gob =
            (y / (gobs_in_y * block_height)) * gobs_size * block_height * image_width_in_gobs +
            (y % (gobs_in_y * block_height) / gobs_in_y) * gobs_size;
        const std::size_t pixel_base{y * width * bytes_per_pixel};
        const auto& table = swizzle_table[y % gobs_in_y];
        for (std::size_t xb = 0; xb < stride; xb += copy_size) {
            const std::size_t truncated_copy = std::min(copy_size, stride - xb);
            const std::size_t gob_address{initial_gob +
                                          (xb / gobs_in_x) * gobs_size * block_height};
            const std::size_t swizzle_offset{gob_address + table[(xb / 16) % 4]};
            const std::size_t pixel_index{xb + pixel_base};
            data_ptrs[unswizzle] = swizzled_data + swizzle_offset;
            data_ptrs[!unswizzle] = unswizzled_data + pixel_index;
            std::memcpy(data_ptrs[0], data_ptrs[1], truncated_copy);
        }
    }
}

u32 BytesPerPixel(TextureFormat format) {
    switch (format) {
    case TextureFormat::DXT1:
    case TextureFormat::DXN1:
        // In this case a 'pixel' actually refers to a 4x4 tile.
        return 8;
    case TextureFormat::DXT23:
    case TextureFormat::DXT45:
    case TextureFormat::DXN2:
    case TextureFormat::BC7U:
    case TextureFormat::BC6H_UF16:
    case TextureFormat::BC6H_SF16:
        // In this case a 'pixel' actually refers to a 4x4 tile.
        return 16;
    case TextureFormat::R32_G32_B32:
        return 12;
    case TextureFormat::ASTC_2D_4X4:
    case TextureFormat::ASTC_2D_8X8:
    case TextureFormat::A8R8G8B8:
    case TextureFormat::A2B10G10R10:
    case TextureFormat::BF10GF11RF11:
    case TextureFormat::R32:
    case TextureFormat::R16_G16:
        return 4;
    case TextureFormat::A1B5G5R5:
    case TextureFormat::B5G6R5:
    case TextureFormat::G8R8:
    case TextureFormat::R16:
        return 2;
    case TextureFormat::R8:
        return 1;
    case TextureFormat::R16_G16_B16_A16:
        return 8;
    case TextureFormat::R32_G32_B32_A32:
        return 16;
    case TextureFormat::R32_G32:
        return 8;
    default:
        UNIMPLEMENTED_MSG("Format not implemented");
        break;
    }
}

std::vector<u8> UnswizzleTexture(VAddr address, u32 tile_size, u32 bytes_per_pixel, u32 width,
                                 u32 height, u32 block_height) {
    std::vector<u8> unswizzled_data(width * height * bytes_per_pixel);
    if (bytes_per_pixel % 3 != 0) {
        FastSwizzleData(width / tile_size, height / tile_size, bytes_per_pixel,
                        Memory::GetPointer(address), unswizzled_data.data(), true, block_height);
    } else {
        CopySwizzledData(width / tile_size, height / tile_size, bytes_per_pixel, bytes_per_pixel,
                         Memory::GetPointer(address), unswizzled_data.data(), true, block_height);
    }
    return unswizzled_data;
}

std::vector<u8> DecodeTexture(const std::vector<u8>& texture_data, TextureFormat format, u32 width,
                              u32 height) {
    std::vector<u8> rgba_data;

    // TODO(Subv): Implement.
    switch (format) {
    case TextureFormat::DXT1:
    case TextureFormat::DXT23:
    case TextureFormat::DXT45:
    case TextureFormat::DXN1:
    case TextureFormat::DXN2:
    case TextureFormat::BC7U:
    case TextureFormat::BC6H_UF16:
    case TextureFormat::BC6H_SF16:
    case TextureFormat::ASTC_2D_4X4:
    case TextureFormat::ASTC_2D_8X8:
    case TextureFormat::A8R8G8B8:
    case TextureFormat::A2B10G10R10:
    case TextureFormat::A1B5G5R5:
    case TextureFormat::B5G6R5:
    case TextureFormat::R8:
    case TextureFormat::G8R8:
    case TextureFormat::BF10GF11RF11:
    case TextureFormat::R32_G32_B32_A32:
    case TextureFormat::R32_G32:
    case TextureFormat::R32:
    case TextureFormat::R16:
    case TextureFormat::R16_G16:
    case TextureFormat::R32_G32_B32:
        // TODO(Subv): For the time being just forward the same data without any decoding.
        rgba_data = texture_data;
        break;
    default:
        UNIMPLEMENTED_MSG("Format not implemented");
        break;
    }

    return rgba_data;
}

} // namespace Tegra::Texture
