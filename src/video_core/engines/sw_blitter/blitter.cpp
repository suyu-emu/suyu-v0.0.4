// SPDX-FileCopyrightText: Copyright 2022 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <vector>

#include "video_core/engines/sw_blitter/blitter.h"
#include "video_core/engines/sw_blitter/converter.h"
#include "video_core/memory_manager.h"
#include "video_core/surface.h"
#include "video_core/textures/decoders.h"

namespace Tegra {
class MemoryManager;
}

using VideoCore::Surface::BytesPerBlock;
using VideoCore::Surface::PixelFormatFromRenderTargetFormat;

namespace Tegra::Engines::Blitter {

using namespace Texture;

namespace {

void NeighrestNeighbor(std::span<u8> input, std::span<u8> output, u32 src_width, u32 src_height,
                       u32 dst_width, u32 dst_height, size_t bpp) {
    const size_t dx_du = std::llround((static_cast<f64>(src_width) / dst_width) * (1ULL << 32));
    const size_t dy_dv = std::llround((static_cast<f64>(src_height) / dst_height) * (1ULL << 32));
    size_t src_y = 0;
    for (u32 y = 0; y < dst_height; y++) {
        size_t src_x = 0;
        for (u32 x = 0; x < dst_width; x++) {
            const size_t read_from = ((src_y * src_width + src_x) >> 32) * bpp;
            const size_t write_to = (y * dst_width + x) * bpp;

            std::memcpy(&output[write_to], &input[read_from], bpp);
            src_x += dx_du;
        }
        src_y += dy_dv;
    }
}

void NeighrestNeighborFast(std::span<f32> input, std::span<f32> output, u32 src_width,
                           u32 src_height, u32 dst_width, u32 dst_height) {
    const size_t dx_du = std::llround((static_cast<f64>(src_width) / dst_width) * (1ULL << 32));
    const size_t dy_dv = std::llround((static_cast<f64>(src_height) / dst_height) * (1ULL << 32));
    size_t src_y = 0;
    for (u32 y = 0; y < dst_height; y++) {
        size_t src_x = 0;
        for (u32 x = 0; x < dst_width; x++) {
            const size_t read_from = ((src_y * src_width + src_x) >> 32) * 4;
            const size_t write_to = (y * dst_width + x) * 4;

            std::memcpy(&output[write_to], &input[read_from], sizeof(f32) * 4);
            src_x += dx_du;
        }
        src_y += dy_dv;
    }
}

/*
void Bilinear(std::span<f32> input, std::span<f32> output, size_t src_width,
                       size_t src_height, size_t dst_width, size_t dst_height) {
    const auto inv_lerp = [](u32 coord, u32 end) { return
static_cast<f32>(std::min(std::max(static_cast<s32>(coord), 0), end - 1)) / (end); };


    for (u32 y = 0; y < dst_height; y++) {
        const f32 ty_0 = inv_lerp(y, dst_extent_y);
        const f32 ty_1 = inv_lerp(y + 1, dst_extent_y);
        for (u32 x = 0; x < dst_width; x++) {
            const f32 tx_0 = inv_lerp(x, dst_extent_x);
            const f32 tx_1 = inv_lerp(x + 1, dst_extent_x);
            const std::array<f32, 4> get_pixel = [&](f32 tx, f32 ty, u32 width, u32 height) {
                std::array<f32, 4> result{};

                return (std::llround(width * tx) + std::llround(height * ty) * width) * 4;
            };
            std::array<f32, 4> result{};

            const size_t read_from = get_pixel(src_width, src_height);
            const size_t write_to = get_pixel(tx_0, ty_0, dst_width, dst_height);

            std::memcpy(&output[write_to], &input[read_from], bpp);
        }
    }
}
*/

} // namespace

struct SoftwareBlitEngine::BlitEngineImpl {
    std::vector<u8> tmp_buffer;
    std::vector<u8> src_buffer;
    std::vector<u8> dst_buffer;
    std::vector<f32> intermediate_src;
    std::vector<f32> intermediate_dst;
    ConverterFactory converter_factory;
};

SoftwareBlitEngine::SoftwareBlitEngine(MemoryManager& memory_manager_)
    : memory_manager{memory_manager_} {
    impl = std::make_unique<BlitEngineImpl>();
}

SoftwareBlitEngine::~SoftwareBlitEngine() = default;

bool SoftwareBlitEngine::Blit(Fermi2D::Surface& src, Fermi2D::Surface& dst,
                              Fermi2D::Config& config) {
    UNIMPLEMENTED_IF(config.filter == Fermi2D::Filter::Bilinear);

    const auto get_surface_size = [](Fermi2D::Surface& surface, u32 bytes_per_pixel) {
        if (surface.linear == Fermi2D::MemoryLayout::BlockLinear) {
            return CalculateSize(true, bytes_per_pixel, surface.width, surface.height,
                                 surface.depth, surface.block_height, surface.block_depth);
        }
        return static_cast<size_t>(surface.pitch * surface.height);
    };
    const auto process_pitch_linear = [](bool unpack, std::span<u8> input, std::span<u8> output,
                                         u32 extent_x, u32 extent_y, u32 pitch, u32 x0, u32 y0,
                                         size_t bpp) {
        const size_t base_offset = x0 * bpp;
        const size_t copy_size = extent_x * bpp;
        for (u32 y = y0; y < extent_y; y++) {
            const size_t first_offset = y * pitch + base_offset;
            const size_t second_offset = y * extent_x * bpp;
            u8* write_to = unpack ? &output[first_offset] : &output[second_offset];
            const u8* read_from = unpack ? &input[second_offset] : &input[first_offset];
            std::memcpy(write_to, read_from, copy_size);
        }
    };

    const u32 src_extent_x = config.src_x1 - config.src_x0;
    const u32 src_extent_y = config.src_y1 - config.src_y0;

    const u32 dst_extent_x = config.dst_x1 - config.dst_x0;
    const u32 dst_extent_y = config.dst_y1 - config.dst_y0;
    const auto src_bytes_per_pixel = BytesPerBlock(PixelFormatFromRenderTargetFormat(src.format));
    const auto dst_bytes_per_pixel = BytesPerBlock(PixelFormatFromRenderTargetFormat(dst.format));
    const size_t src_size = get_surface_size(src, src_bytes_per_pixel);
    impl->tmp_buffer.resize(src_size);
    memory_manager.ReadBlock(src.Address(), impl->tmp_buffer.data(), src_size);

    const size_t src_copy_size = src_extent_x * src_extent_y * src_bytes_per_pixel;

    const size_t dst_copy_size = dst_extent_x * dst_extent_y * dst_bytes_per_pixel;

    impl->src_buffer.resize(src_copy_size);

    const bool no_passthrough =
        src.format != dst.format || src_extent_x != dst_extent_x || src_extent_y != dst_extent_y;

    const auto convertion_phase_same_format = [&]() {
        NeighrestNeighbor(impl->src_buffer, impl->dst_buffer, src_extent_x, src_extent_y,
                          dst_extent_x, dst_extent_y, dst_bytes_per_pixel);
    };

    const auto convertion_phase_ir = [&]() {
        auto* input_converter = impl->converter_factory.GetFormatConverter(src.format);
        impl->intermediate_src.resize((src_copy_size / src_bytes_per_pixel) * 4);
        impl->intermediate_dst.resize((dst_copy_size / dst_bytes_per_pixel) * 4);
        input_converter->ConvertTo(impl->src_buffer, impl->intermediate_src);

        NeighrestNeighborFast(impl->intermediate_src, impl->intermediate_dst, src_extent_x,
                              src_extent_y, dst_extent_x, dst_extent_y);

        auto* output_converter = impl->converter_factory.GetFormatConverter(dst.format);
        output_converter->ConvertFrom(impl->intermediate_dst, impl->dst_buffer);
    };

    // Do actuall Blit

    impl->dst_buffer.resize(dst_copy_size);
    if (src.linear == Fermi2D::MemoryLayout::BlockLinear) {
        UnswizzleSubrect(impl->src_buffer, impl->tmp_buffer, src_bytes_per_pixel, src.width,
                         src.height, src.depth, config.src_x0, config.src_y0, src_extent_x,
                         src_extent_y, src.block_height, src.block_depth,
                         src_extent_x * src_bytes_per_pixel);
    } else {
        process_pitch_linear(false, impl->tmp_buffer, impl->src_buffer, src_extent_x, src_extent_y,
                             src.pitch, config.src_x0, config.src_y0, src_bytes_per_pixel);
    }

    // Conversion Phase
    if (no_passthrough) {
        if (src.format != dst.format) {
            convertion_phase_ir();
        } else {
            convertion_phase_same_format();
        }
    } else {
        impl->dst_buffer.swap(impl->src_buffer);
    }

    const size_t dst_size = get_surface_size(dst, dst_bytes_per_pixel);
    impl->tmp_buffer.resize(dst_size);
    memory_manager.ReadBlock(dst.Address(), impl->tmp_buffer.data(), dst_size);

    if (dst.linear == Fermi2D::MemoryLayout::BlockLinear) {
        SwizzleSubrect(impl->tmp_buffer, impl->dst_buffer, dst_bytes_per_pixel, dst.width,
                       dst.height, dst.depth, config.dst_x0, config.dst_y0, dst_extent_x,
                       dst_extent_y, dst.block_height, dst.block_depth,
                       dst_extent_x * dst_bytes_per_pixel);
    } else {
        process_pitch_linear(true, impl->dst_buffer, impl->tmp_buffer, dst_extent_x, dst_extent_y,
                             dst.pitch, config.dst_x0, config.dst_y0,
                             static_cast<size_t>(dst_bytes_per_pixel));
    }
    memory_manager.WriteBlock(dst.Address(), impl->tmp_buffer.data(), dst_size);
    return true;
}

} // namespace Tegra::Engines::Blitter
