// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include <vector>

#include "video_core/engines/maxwell_3d.h"
#include "video_core/renderer_vulkan/maxwell_to_vk.h"
#include "video_core/renderer_vulkan/vk_device.h"
#include "video_core/renderer_vulkan/vk_renderpass_cache.h"
#include "video_core/renderer_vulkan/wrapper.h"

namespace Vulkan {

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
    std::vector<VkAttachmentDescription> descriptors;
    std::vector<VkAttachmentReference> color_references;

    for (std::size_t rt = 0; rt < params.color_attachments.size(); ++rt) {
        const auto attachment = params.color_attachments[rt];
        const auto format =
            MaxwellToVK::SurfaceFormat(device, FormatType::Optimal, attachment.pixel_format);
        ASSERT_MSG(format.attachable, "Trying to attach a non-attachable format with format={}",
                   static_cast<u32>(attachment.pixel_format));

        // TODO(Rodrigo): Add eMayAlias when it's needed.
        const auto color_layout = attachment.is_texception
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
    if (params.has_zeta) {
        const auto format =
            MaxwellToVK::SurfaceFormat(device, FormatType::Optimal, params.zeta_pixel_format);
        ASSERT_MSG(format.attachable, "Trying to attach a non-attachable format with format={}",
                   static_cast<u32>(params.zeta_pixel_format));

        const auto zeta_layout = params.zeta_texception
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

        zeta_attachment_ref.attachment = static_cast<u32>(params.color_attachments.size());
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
    subpass_description.pDepthStencilAttachment = params.has_zeta ? &zeta_attachment_ref : nullptr;
    subpass_description.preserveAttachmentCount = 0;
    subpass_description.pPreserveAttachments = nullptr;

    VkAccessFlags access = 0;
    VkPipelineStageFlags stage = 0;
    if (!color_references.empty()) {
        access |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        stage |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }

    if (params.has_zeta) {
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
