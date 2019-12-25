// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include <vector>

#include "common/assert.h"
#include "video_core/renderer_vulkan/declarations.h"
#include "video_core/renderer_vulkan/vk_device.h"
#include "video_core/renderer_vulkan/vk_image.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"

namespace Vulkan {

VKImage::VKImage(const VKDevice& device, VKScheduler& scheduler,
                 const vk::ImageCreateInfo& image_ci, vk::ImageAspectFlags aspect_mask)
    : device{device}, scheduler{scheduler}, format{image_ci.format}, aspect_mask{aspect_mask},
      image_num_layers{image_ci.arrayLayers}, image_num_levels{image_ci.mipLevels} {
    UNIMPLEMENTED_IF_MSG(image_ci.queueFamilyIndexCount != 0,
                         "Queue family tracking is not implemented");

    const auto dev = device.GetLogical();
    image = dev.createImageUnique(image_ci, nullptr, device.GetDispatchLoader());

    const u32 num_ranges = image_num_layers * image_num_levels;
    barriers.resize(num_ranges);
    subrange_states.resize(num_ranges, {{}, image_ci.initialLayout});
}

VKImage::~VKImage() = default;

void VKImage::Transition(u32 base_layer, u32 num_layers, u32 base_level, u32 num_levels,
                         vk::PipelineStageFlags new_stage_mask, vk::AccessFlags new_access,
                         vk::ImageLayout new_layout) {
    if (!HasChanged(base_layer, num_layers, base_level, num_levels, new_access, new_layout)) {
        return;
    }

    std::size_t cursor = 0;
    for (u32 layer_it = 0; layer_it < num_layers; ++layer_it) {
        for (u32 level_it = 0; level_it < num_levels; ++level_it, ++cursor) {
            const u32 layer = base_layer + layer_it;
            const u32 level = base_level + level_it;
            auto& state = GetSubrangeState(layer, level);
            barriers[cursor] = vk::ImageMemoryBarrier(
                state.access, new_access, state.layout, new_layout, VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED, *image, {aspect_mask, level, 1, layer, 1});
            state.access = new_access;
            state.layout = new_layout;
        }
    }

    scheduler.RequestOutsideRenderPassOperationContext();

    scheduler.Record([barriers = barriers, cursor](auto cmdbuf, auto& dld) {
        // TODO(Rodrigo): Implement a way to use the latest stage across subresources.
        constexpr auto stage_stub = vk::PipelineStageFlagBits::eAllCommands;
        cmdbuf.pipelineBarrier(stage_stub, stage_stub, {}, 0, nullptr, 0, nullptr,
                               static_cast<u32>(cursor), barriers.data(), dld);
    });
}

bool VKImage::HasChanged(u32 base_layer, u32 num_layers, u32 base_level, u32 num_levels,
                         vk::AccessFlags new_access, vk::ImageLayout new_layout) noexcept {
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
    const vk::ImageViewCreateInfo image_view_ci({}, *image, vk::ImageViewType::e2D, format, {},
                                                {aspect_mask, 0, 1, 0, 1});
    const auto dev = device.GetLogical();
    const auto& dld = device.GetDispatchLoader();
    present_view = dev.createImageViewUnique(image_view_ci, nullptr, dld);
}

VKImage::SubrangeState& VKImage::GetSubrangeState(u32 layer, u32 level) noexcept {
    return subrange_states[static_cast<std::size_t>(layer * image_num_levels) +
                           static_cast<std::size_t>(level)];
}

} // namespace Vulkan