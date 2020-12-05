// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include <memory>
#include <vector>

#include "common/cityhash.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/renderer_vulkan/maxwell_to_vk.h"
#include "video_core/renderer_vulkan/vk_device.h"
#include "video_core/renderer_vulkan/vk_renderpass_cache.h"
#include "video_core/renderer_vulkan/wrapper.h"

namespace Vulkan {

std::size_t RenderPassParams::Hash() const noexcept {
    const u64 hash = Common::CityHash64(reinterpret_cast<const char*>(this), sizeof *this);
    return static_cast<std::size_t>(hash);
}

bool RenderPassParams::operator==(const RenderPassParams& rhs) const noexcept {
    return std::memcmp(&rhs, this, sizeof *this) == 0;
}

VKRenderPassCache::VKRenderPassCache(const VKDevice& device_) : device{device_} {}

VKRenderPassCache::~VKRenderPassCache() = default;

VkRenderPass VKRenderPassCache::GetRenderPass(const RenderPassParams& params) {
    const auto [pair, is_cache_miss] = cache.try_emplace(params);
    auto& entry = pair->second;
    if (is_cache_miss) {
        entry = CreateRenderPass(params);
    }
    return *entry;
}

vk::RenderPass VKRenderPassCache::CreateRenderPass(const RenderPassParams& params) const {
    using namespace VideoCore::Surface;
    const std::size_t num_attachments = static_cast<std::size_t>(params.num_color_attachments);

    std::vector<VkAttachmentDescription> descriptors;
    descriptors.reserve(num_attachments);

    std::vector<VkAttachmentReference> color_references;
    color_references.reserve(num_attachments);

    for (std::size_t rt = 0; rt < num_attachments; ++rt) {
        const auto guest_format = static_cast<Tegra::RenderTargetFormat>(params.color_formats[rt]);
        const PixelFormat pixel_format = PixelFormatFromRenderTargetFormat(guest_format);
        const auto format = MaxwellToVK::SurfaceFormat(device, FormatType::Optimal, pixel_format);
        ASSERT_MSG(format.attachable, "Trying to attach a non-attachable format with format={}",
                   static_cast<int>(pixel_format));

        // TODO(Rodrigo): Add MAY_ALIAS_BIT when it's needed.
        const VkImageLayout color_layout = ((params.texceptions >> rt) & 1) != 0
                                               ? VK_IMAGE_LAYOUT_GENERAL
                                               : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        descriptors.push_back({
            .flags = VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT,
            .format = format.format,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = color_layout,
            .finalLayout = color_layout,
        });

        color_references.push_back({
            .attachment = static_cast<u32>(rt),
            .layout = color_layout,
        });
    }

    VkAttachmentReference zeta_attachment_ref;
    const bool has_zeta = params.zeta_format != 0;
    if (has_zeta) {
        const auto guest_format = static_cast<Tegra::DepthFormat>(params.zeta_format);
        const PixelFormat pixel_format = PixelFormatFromDepthFormat(guest_format);
        const auto format = MaxwellToVK::SurfaceFormat(device, FormatType::Optimal, pixel_format);
        ASSERT_MSG(format.attachable, "Trying to attach a non-attachable format with format={}",
                   static_cast<int>(pixel_format));

        const VkImageLayout zeta_layout = params.zeta_texception != 0
                                              ? VK_IMAGE_LAYOUT_GENERAL
                                              : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        descriptors.push_back({
            .flags = 0,
            .format = format.format,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = zeta_layout,
            .finalLayout = zeta_layout,
        });

        zeta_attachment_ref = {
            .attachment = static_cast<u32>(num_attachments),
            .layout = zeta_layout,
        };
    }

    const VkSubpassDescription subpass_description{
        .flags = 0,
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .inputAttachmentCount = 0,
        .pInputAttachments = nullptr,
        .colorAttachmentCount = static_cast<u32>(color_references.size()),
        .pColorAttachments = color_references.data(),
        .pResolveAttachments = nullptr,
        .pDepthStencilAttachment = has_zeta ? &zeta_attachment_ref : nullptr,
        .preserveAttachmentCount = 0,
        .pPreserveAttachments = nullptr,
    };

    VkAccessFlags access = 0;
    VkPipelineStageFlags stage = 0;
    if (!color_references.empty()) {
        access |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        stage |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }

    if (has_zeta) {
        access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        stage |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    }

    const VkSubpassDependency subpass_dependency{
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = stage,
        .dstStageMask = stage,
        .srcAccessMask = 0,
        .dstAccessMask = access,
        .dependencyFlags = 0,
    };

    return device.GetLogical().CreateRenderPass({
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .attachmentCount = static_cast<u32>(descriptors.size()),
        .pAttachments = descriptors.data(),
        .subpassCount = 1,
        .pSubpasses = &subpass_description,
        .dependencyCount = 1,
        .pDependencies = &subpass_dependency,
    });
}

} // namespace Vulkan
