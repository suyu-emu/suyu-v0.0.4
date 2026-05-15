// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include <fmt/ranges.h>

#include "video_core/surface.h"
#include "video_core/texture_cache/types.h"

template <>
struct fmt::formatter<VideoCore::Surface::PixelFormat> : fmt::formatter<fmt::string_view> {
    template <typename FormatContext>
    auto format(VideoCore::Surface::PixelFormat format, FormatContext& ctx) const {
        using VideoCore::Surface::PixelFormat;
        const string_view name = [format] {
            switch (format) {
#define PIXEL_FORMAT_ELEM(NAME, ...) case PixelFormat::NAME: return #NAME;
    PIXEL_FORMAT_LIST
#undef PIXEL_FORMAT_ELEM
            case PixelFormat::MaxDepthStencilFormat:
            case PixelFormat::Invalid:
                return "Invalid";
            }
            return "Invalid";
        }();
        return formatter<string_view>::format(name, ctx);
    }
};

template <>
struct fmt::formatter<VideoCommon::ImageType> : fmt::formatter<fmt::string_view> {
    template <typename FormatContext>
    auto format(VideoCommon::ImageType type, FormatContext& ctx) const {
        const string_view name = [type] {
            using VideoCommon::ImageType;
            switch (type) {
            case ImageType::e1D:
                return "1D";
            case ImageType::e2D:
                return "2D";
            case ImageType::e3D:
                return "3D";
            case ImageType::Linear:
                return "Linear";
            case ImageType::Buffer:
                return "Buffer";
            }
            return "Invalid";
        }();
        return formatter<string_view>::format(name, ctx);
    }
};

template <>
struct fmt::formatter<VideoCommon::Extent3D> {
    constexpr auto parse(fmt::format_parse_context& ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const VideoCommon::Extent3D& extent, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "{{{}, {}, {}}}", extent.width, extent.height,
                              extent.depth);
    }
};

namespace VideoCommon {

struct ImageBase;
struct ImageViewBase;
struct RenderTargets;

[[nodiscard]] std::string Name(const ImageBase& image);

[[nodiscard]] std::string Name(const ImageViewBase& image_view, GPUVAddr addr);

[[nodiscard]] std::string Name(const RenderTargets& render_targets);

} // namespace VideoCommon
