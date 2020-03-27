// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include <vector>

#include "common/assert.h"
#include "video_core/renderer_vulkan/vk_device.h"
#include "video_core/renderer_vulkan/vk_image.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/wrapper.h"

namespace Vulkan {

VKImage::VKImage(const VKDevice& device, VKScheduler& scheduler, const VkImageCreateInfo& image_ci,
                 VkImageAspectFlags aspect_mask)
    : device{device}, scheduler{scheduler}, format{image_ci.format}, aspect_mask{aspect_mask},
      image_num_layers{image_ci.arrayLayers}, image_num_levels{image_ci.mipLevels} {
    UNIMPLEMENTED_IF_MSG(image_ci.queueFamilyIndexCount != 0,
                         "Queue family tracking is not implemented");

    image = device.GetLogical().CreateImage(image_ci);

    const u32 num_ranges = image_num_layers * image_num_levels;
    barriers.resize(num_ranges);
    subrange_states.resize(num_ranges, {{}, image_ci.initialLayout});
}

VKImage::~VKImage() = default;

void VKImage::Transition(u32 base_layer, u32 num_layers, u32 base_level, u32 num_levels,
                         VkPipelineStageFlags new_stage_mask, VkAccessFlags new_access,
                         VkImageLayout new_layout) {
    if (!HasChanged(base_layer, num_layers, base_level, num_levels, new_access, new_layout)) {
        return;
    }

    std::size_t cursor = 0;
    for (u32 layer_it = 0; layer_it < num_layers; ++layer_it) {
        for (u32 level_it = 0; level_it < num_levels; ++level_it, ++cursor) {
            const u32 layer = base_layer + layer_it;
            const u32 level = base_level + level_it;
            auto& state = GetSubrangeState(layer, level);
            auto& barrier = barriers[cursor];
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.pNext = nullptr;
            barrier.srcAccessMask = state.access;
            barrier.dstAccessMask = new_access;
            barrier.oldLayout = state.layout;
            barrier.newLayout = new_layout;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = *image;
            barrier.subresourceRange.aspectMask = aspect_mask;
            barrier.subresourceRange.baseMipLevel = level;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = layer;
            barrier.subresourceRange.layerCount = 1;
            state.access = new_access;
            state.layout = new_layout;
        }
    }

    scheduler.RequestOutsideRenderPassOperationContext();

    scheduler.Record([barriers = barriers, cursor](vk::CommandBuffer cmdbuf) {
        // TODO(Rodrigo): Implement a way to use the latest stage across subresources.
        cmdbuf.PipelineBarrier(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                               VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, {}, {},
                               vk::Span(barriers.data(), cursor));
    });
}

bool VKImage::HasChanged(u32 base_layer, u32 num_layers, u32 base_level, u32 num_levels,
                         VkAccessFlags new_access, VkImageLayout new_layout) noexcept {
    const bool is_full_range = base_layer == 0 && num_layers == image_num_layers &&
                               base_level == 0 && num_levels == image_num_levels;
    if (!is_full_range) {
        state_diverged = true;
    }

    if (!state_diverged) {
        auto& state = GetSubrangeState(0, 0);
        if (state.access != new_access || state.layout != new_layout) {
            return true;
        }
    }

    for (u32 layer_it = 0; layer_it < num_layers; ++layer_it) {
        for (u32 level_it = 0; level_it < num_levels; ++level_it) {
            const u32 layer = base_layer + layer_it;
            const u32 level = base_level + level_it;
            auto& state = GetSubrangeState(layer, level);
            if (state.access != new_access || state.layout != new_layout) {
                return true;
            }
        }
    }
    return false;
}

void VKImage::CreatePresentView() {
    // Image type has to be 2D to be presented.
    VkImageViewCreateInfo image_view_ci;
    image_view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_ci.pNext = nullptr;
    image_view_ci.flags = 0;
    image_view_ci.image = *image;
    image_view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_ci.format = format;
    image_view_ci.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
    image_view_ci.subresourceRange.aspectMask = aspect_mask;
    image_view_ci.subresourceRange.baseMipLevel = 0;
    image_view_ci.subresourceRange.levelCount = 1;
    image_view_ci.subresourceRange.baseArrayLayer = 0;
    image_view_ci.subresourceRange.layerCount = 1;
    present_view = device.GetLogical().CreateImageView(image_view_ci);
}

VKImage::SubrangeState& VKImage::GetSubrangeState(u32 layer, u32 level) noexcept {
    return subrange_states[static_cast<std::size_t>(layer * image_num_levels) +
                           static_cast<std::size_t>(level)];
}

} // namespace Vulkan