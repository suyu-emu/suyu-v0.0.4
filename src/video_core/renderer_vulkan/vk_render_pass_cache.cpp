// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <ankerl/unordered_dense.h>

#include <boost/container/static_vector.hpp>

#include "video_core/renderer_vulkan/maxwell_to_vk.h"
#include "video_core/renderer_vulkan/vk_render_pass_cache.h"
#include "video_core/surface.h"
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {
namespace {
using VideoCore::Surface::PixelFormat;
using VideoCore::Surface::SurfaceType;

        constexpr SurfaceType GetSurfaceType(PixelFormat format) {
            switch (format) {
                // Depth formats
                case PixelFormat::D16_UNORM:
                case PixelFormat::D32_FLOAT:
                case PixelFormat::X8_D24_UNORM:
                    return SurfaceType::Depth;

                    // Stencil formats
                case PixelFormat::S8_UINT:
                    return SurfaceType::Stencil;

                    // Depth+Stencil formats
                case PixelFormat::D24_UNORM_S8_UINT:
                case PixelFormat::S8_UINT_D24_UNORM:
                case PixelFormat::D32_FLOAT_S8_UINT:
                    return SurfaceType::DepthStencil;

                    // Everything else is a color texture
                default:
                    return SurfaceType::ColorTexture;
            }
        }

        VkAttachmentDescription AttachmentDescription(const Device& device, PixelFormat format,
                                                      VkSampleCountFlagBits samples,
                                                      VkAttachmentLoadOp load_op,
                                                      VkAttachmentStoreOp store_op) {
            using MaxwellToVK::SurfaceFormat;

            const SurfaceType surface_type = GetSurfaceType(format);
            const bool has_stencil = surface_type == SurfaceType::DepthStencil ||
                                     surface_type == SurfaceType::Stencil;

            return {
                .flags = {},
                .format = SurfaceFormat(device, FormatType::Optimal, true, format).format,
                .samples = samples,
                .loadOp = load_op,
                .storeOp = store_op,
                .stencilLoadOp = has_stencil ? load_op : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = has_stencil ? store_op : VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_GENERAL,
                .finalLayout = VK_IMAGE_LAYOUT_GENERAL,
            };
        }
    } // Anonymous namespace

RenderPassCache::RenderPassCache(const Device& device_) : device{&device_} {}

VkRenderPass RenderPassCache::Get(const RenderPassKey& key) {
    std::scoped_lock lock{mutex};
    const auto [pair, is_new] = cache.try_emplace(key);
    if (!is_new) {
        return *pair->second;
    }
    boost::container::static_vector<VkAttachmentDescription, 9> descriptions;
    std::array<VkAttachmentReference, 8> references{};
    u32 num_attachments{};
    u32 num_colors{};
    for (size_t index = 0; index < key.color_formats.size(); ++index) {
        const PixelFormat format{key.color_formats[index]};
        const bool is_valid{format != PixelFormat::Invalid};
        references[index] = VkAttachmentReference{
            .attachment = is_valid ? num_colors : VK_ATTACHMENT_UNUSED,
            .layout = VK_IMAGE_LAYOUT_GENERAL,
        };
        if (is_valid) {
            const VkAttachmentLoadOp load_op = (key.color_clear_mask & (1u << index)) != 0
                                                   ? VK_ATTACHMENT_LOAD_OP_CLEAR
                                                   : VK_ATTACHMENT_LOAD_OP_LOAD;
            const VkAttachmentStoreOp store_op = (key.color_discard_mask & (1u << index)) != 0
                                                     ? VK_ATTACHMENT_STORE_OP_DONT_CARE
                                                     : VK_ATTACHMENT_STORE_OP_STORE;
            descriptions.push_back(
                AttachmentDescription(*device, format, key.samples, load_op, store_op));
            num_attachments = static_cast<u32>(index + 1);
            ++num_colors;
        }
    }
    const bool has_depth{key.depth_format != PixelFormat::Invalid};
    VkAttachmentReference depth_reference{};
    if (key.depth_format != PixelFormat::Invalid) {
        depth_reference = VkAttachmentReference{
            .attachment = num_colors,
            .layout = VK_IMAGE_LAYOUT_GENERAL,
        };
        const VkAttachmentLoadOp depth_load_op = key.depth_stencil_clear
                                                     ? VK_ATTACHMENT_LOAD_OP_CLEAR
                                                     : VK_ATTACHMENT_LOAD_OP_LOAD;
        descriptions.push_back(AttachmentDescription(*device, key.depth_format, key.samples,
                                                     depth_load_op, VK_ATTACHMENT_STORE_OP_STORE));
    }
    std::array<VkAttachmentReference, 8> resolve_references{};
    const bool do_resolve_color =
        key.resolve_color && key.samples != VK_SAMPLE_COUNT_1_BIT && num_colors > 0;
    if (do_resolve_color) {
        for (size_t index = 0; index < key.color_formats.size(); ++index) {
            const PixelFormat format{key.color_formats[index]};
            const bool is_valid{format != PixelFormat::Invalid};
            resolve_references[index] = VkAttachmentReference{
                .attachment = is_valid ? static_cast<u32>(descriptions.size()) : VK_ATTACHMENT_UNUSED,
                .layout = VK_IMAGE_LAYOUT_GENERAL,
            };
            if (is_valid) {
                VkAttachmentDescription resolve_desc =
                    AttachmentDescription(*device, format, VK_SAMPLE_COUNT_1_BIT,
                                          VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                          VK_ATTACHMENT_STORE_OP_STORE);
                resolve_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                descriptions.push_back(resolve_desc);
            }
        }
    }
    const VkSubpassDescription subpass{
        .flags = 0,
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .inputAttachmentCount = 0,
        .pInputAttachments = nullptr,
        .colorAttachmentCount = num_attachments,
        .pColorAttachments = references.data(),
        .pResolveAttachments = do_resolve_color ? resolve_references.data() : nullptr,
        .pDepthStencilAttachment = has_depth ? &depth_reference : nullptr,
        .preserveAttachmentCount = 0,
        .pPreserveAttachments = nullptr,
    };
    const VkSubpassDependency dependency{
            .srcSubpass = 0,  // Current subpass
            .dstSubpass = 0,  // Same subpass (self-dependency)
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
    };
    pair->second = device->GetLogical().CreateRenderPass({
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .attachmentCount = static_cast<u32>(descriptions.size()),
        .pAttachments = descriptions.empty() ? nullptr : descriptions.data(),
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency,
    });
    return *pair->second;
}

} // namespace Vulkan
