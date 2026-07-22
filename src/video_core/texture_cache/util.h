// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <span>
#include <boost/container/small_vector.hpp>

#include "common/common_types.h"
#include "common/scratch_buffer.h"

#include "video_core/surface.h"
#include "video_core/texture_cache/image_base.h"
#include "video_core/texture_cache/types.h"
#include "video_core/textures/texture.h"

namespace VideoCommon {

using Tegra::Texture::TICEntry;

using LevelArray = std::array<u32, MAX_MIP_LEVELS>;

struct OverlapResult {
    GPUVAddr gpu_addr;
    VAddr cpu_addr;
    SubresourceExtent resources;
};

[[nodiscard]] u32 CalculateGuestSizeInBytes(const ImageInfo& info) noexcept;

[[nodiscard]] u32 CalculateUnswizzledSizeBytes(const ImageInfo& info) noexcept;

[[nodiscard]] u32 CalculateConvertedSizeBytes(const ImageInfo& info) noexcept;

[[nodiscard]] u32 CalculateLayerStride(const ImageInfo& info) noexcept;

[[nodiscard]] u32 CalculateLayerSize(const ImageInfo& info) noexcept;

[[nodiscard]] LevelArray CalculateMipLevelOffsets(const ImageInfo& info) noexcept;

[[nodiscard]] LevelArray CalculateMipLevelSizes(const ImageInfo& info) noexcept;

[[nodiscard]] boost::container::small_vector<u32, 16> CalculateSliceOffsets(const ImageInfo& info);

[[nodiscard]] boost::container::small_vector<SubresourceBase, 16> CalculateSliceSubresources(
    const ImageInfo& info);

[[nodiscard]] u32 CalculateLevelStrideAlignment(const ImageInfo& info, u32 level);

[[nodiscard]] VideoCore::Surface::PixelFormat PixelFormatFromTIC(
    const Tegra::Texture::TICEntry& config) noexcept;

[[nodiscard]] ImageViewType RenderTargetImageViewType(const ImageInfo& info) noexcept;

[[nodiscard]] boost::container::small_vector<ImageCopy, 16> MakeShrinkImageCopies(
    const ImageInfo& dst, const ImageInfo& src, SubresourceBase base, u32 up_scale = 1,
    u32 down_shift = 0);

[[nodiscard]] boost::container::small_vector<ImageCopy, 16> MakeReinterpretImageCopies(
    const ImageInfo& src, u32 up_scale = 1, u32 down_shift = 0);

[[nodiscard]] bool IsValidEntry(const Tegra::MemoryManager& gpu_memory, const TICEntry& config);

[[nodiscard]] boost::container::small_vector<BufferImageCopy, 16> UnswizzleImage(
    Tegra::MemoryManager& gpu_memory, GPUVAddr gpu_addr, const ImageInfo& info,
    std::span<const u8> input, std::span<u8> output);

void ConvertImage(std::span<const u8> input, const ImageInfo& info, std::span<u8> output,
                  std::span<BufferImageCopy> copies);

[[nodiscard]] boost::container::small_vector<BufferImageCopy, 16> FullDownloadCopies(
    const ImageInfo& info);

[[nodiscard]] Extent3D MipSize(Extent3D size, u32 level);

[[nodiscard]] Extent3D MipBlockSize(const ImageInfo& info, u32 level);

[[nodiscard]] boost::container::small_vector<SwizzleParameters, 16> FullUploadSwizzles(
    const ImageInfo& info);

void SwizzleImage(Tegra::MemoryManager& gpu_memory, GPUVAddr gpu_addr, const ImageInfo& info,
                  std::span<const BufferImageCopy> copies, std::span<const u8> memory,
                  Common::ScratchBuffer<u8>& tmp_buffer);

[[nodiscard]] bool IsBlockLinearSizeCompatible(const ImageInfo& new_info,
                                               const ImageInfo& overlap_info, u32 new_level,
                                               u32 overlap_level, bool strict_size) noexcept;

[[nodiscard]] bool IsPitchLinearSameSize(const ImageInfo& lhs, const ImageInfo& rhs,
                                         bool strict_size) noexcept;

[[nodiscard]] bool IsBlockLinearSizeCompatibleBPPRelaxed(const ImageInfo& lhs, const ImageInfo& rhs,
                                                         u32 lhs_level, u32 rhs_level) noexcept;

[[nodiscard]] std::optional<OverlapResult> ResolveOverlap(const ImageInfo& new_info,
                                                          GPUVAddr gpu_addr, VAddr cpu_addr,
                                                          const ImageBase& overlap,
                                                          bool strict_size, bool broken_views,
                                                          bool native_bgr);

[[nodiscard]] bool IsLayerStrideCompatible(const ImageInfo& lhs, const ImageInfo& rhs);

[[nodiscard]] std::optional<SubresourceBase> FindSubresource(const ImageInfo& candidate,
                                                             const ImageBase& image,
                                                             GPUVAddr candidate_addr,
                                                             RelaxedOptions options,
                                                             bool broken_views, bool native_bgr);

[[nodiscard]] bool IsSubresource(const ImageInfo& candidate, const ImageBase& image,
                                 GPUVAddr candidate_addr, RelaxedOptions options, bool broken_views,
                                 bool native_bgr);

[[nodiscard]] bool IsSubCopy(const ImageInfo& candidate, const ImageBase& image,
                             GPUVAddr candidate_addr);

void DeduceBlitImages(ImageInfo& dst_info, ImageInfo& src_info, const ImageBase* dst,
                      const ImageBase* src);

[[nodiscard]] u32 MapSizeBytes(const ImageBase& image);

// TODO: Remove once Debian STABLE no longer has such outdated boost
// This is a gcc bug where ADL lookup fails for range niebloids of std::span<T>
// for any given type of the static_vector/small_vector, etc which makes a whole mess
// for anything using std::span<T> so we just do this terrible hack on older versions of
// GCC12 because people actually still use stable debian so... yeah
// One may say: "This is bad for performance" - to which I say, using GCC 12 you already know
// what kind of bs you will be dealing with anyways.
template<typename T, size_t N>
#if BOOST_VERSION >= 108100 || __GNUC__ > 12
[[nodiscard]] boost::container::small_vector<T, N> FixSmallVectorADL(const boost::container::small_vector<T, N>& v) {
    return v;
}
#else
[[nodiscard]] std::vector<T> FixSmallVectorADL(const boost::container::small_vector<T, N>& v) {
    std::vector<T> u;
    for (auto const& e : v)
        u.push_back(e);
    return u;
}
#endif

} // namespace VideoCommon
