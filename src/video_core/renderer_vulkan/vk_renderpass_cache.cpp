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

VKRenderPassCache::VKRenderPassCache(const VKDevice& device) : device{device} {}

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
    std::vector<VkAttachmentDescription> descriptors;
    std::vector<VkAttachmentReference> color_references;

    const std::size_t num_attachments = static_cast<std::size_t>(params.num_color_attachments);
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
        VkAttachmentDescription& descriptor = descriptors.emplace_back();
        descriptor.flags = VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT;
        descriptor.format = format.format;
        descriptor.samples = VK_SAMPLE_COUNT_1_BIT;
        descriptor.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        descriptor.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        descriptor.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        descriptor.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        descriptor.initialLayout = color_layout;
        descriptor.finalLayout = color_layout;

        VkAttachmentReference& reference = color_references.emplace_back();
        reference.attachment = static_cast<u32>(rt);
        reference.layout = color_layout;
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
        VkAttachmentDescription& descriptor = descriptors.emplace_back();
        descriptor.flags = 0;
        descriptor.format = format.format;
        descriptor.samples = VK_SAMPLE_COUNT_1_BIT;
        descriptor.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        descriptor.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        descriptor.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        descriptor.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
        descriptor.initialLayout = zeta_layout;
        descriptor.finalLayout = zeta_layout;

        zeta_attachment_ref.attachment = static_cast<u32>(num_attachments);
        zeta_attachment_ref.layout = zeta_layout;
    }

    VkSubpassDescription subpass_description;
    subpass_description.flags = 0;
    subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_description.inputAttachmentCount = 0;
    subpass_description.pInputAttachments = nullptr;
    subpass_description.colorAttachmentCount = static_cast<u32>(color_references.size());
    subpass_description.pColorAttachments = color_references.data();
    subpass_description.pResolveAttachments = nullptr;
    subpass_description.pDepthStencilAttachment = has_zeta ? &zeta_attachment_ref : nullptr;
    subpass_description.preserveAttachmentCount = 0;
    subpass_description.pPreserveAttachments = nullptr;

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

    VkSubpassDependency subpass_dependency;
    subpass_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    subpass_dependency.dstSubpass = 0;
    subpass_dependency.srcStageMask = stage;
    subpass_dependency.dstStageMask = stage;
    subpass_dependency.srcAccessMask = 0;
    subpass_dependency.dstAccessMask = access;
    subpass_dependency.dependencyFlags = 0;

    VkRenderPassCreateInfo ci;
    ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    ci.pNext = nullptr;
    ci.flags = 0;
    ci.attachmentCount = static_cast<u32>(descriptors.size());
    ci.pAttachments = descriptors.data();
    ci.subpassCount = 1;
    ci.pSubpasses = &subpass_description;
    ci.dependencyCount = 1;
    ci.pDependencies = &subpass_dependency;
    return device.GetLogical().CreateRenderPass(ci);
}

} // namespace Vulkan
