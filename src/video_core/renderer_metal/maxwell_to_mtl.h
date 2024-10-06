// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <Metal/Metal.hpp>

#include "video_core/engines/maxwell_3d.h"
#include "video_core/surface.h"
#include "video_core/texture_cache/types.h"

namespace Metal::MaxwellToMTL {

using Maxwell = Tegra::Engines::Maxwell3D::Regs;

struct PixelFormatInfo {
    MTL::PixelFormat pixel_format;
    size_t bytes_per_block;
    VideoCommon::Extent2D block_texel_size{1, 1};
    bool can_be_render_target = true;
};

void CheckForPixelFormatSupport(MTL::Device* device);

const PixelFormatInfo& GetPixelFormatInfo(VideoCore::Surface::PixelFormat pixel_format);

size_t GetTextureBytesPerRow(VideoCore::Surface::PixelFormat pixel_format, u32 texels_per_row);

MTL::VertexFormat VertexFormat(Maxwell::VertexAttribute::Type type,
                               Maxwell::VertexAttribute::Size size);

MTL::IndexType IndexType(Maxwell::IndexFormat format);

size_t IndexSize(Maxwell::IndexFormat format);

} // namespace Metal::MaxwellToMTL
