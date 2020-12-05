// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <vector>

#include "common/common_types.h"
#include "video_core/renderer_vulkan/wrapper.h"

namespace Vulkan {

class VKDevice;
class VKScheduler;

class VKImage {
public:
    explicit VKImage(const VKDevice& device_, VKScheduler& scheduler_,
                     const VkImageCreateInfo& image_ci_, VkImageAspectFlags aspect_mask_);
    ~VKImage();

    /// Records in the passed command buffer an image transition and updates the state of the image.
    void Transition(u32 base_layer, u32 num_layers, u32 base_level, u32 num_levels,
                    VkPipelineStageFlags new_stage_mask, VkAccessFlags new_access,
                    VkImageLayout new_layout);

    /// Returns a view compatible with presentation, the image has to be 2D.
    VkImageView GetPresentView() {
        if (!present_view) {
            CreatePresentView();
        }
        return *present_view;
    }

    /// Returns the Vulkan image handler.
    const vk::Image& GetHandle() const {
        return image;
    }

    /// Returns the Vulkan format for this image.
    VkFormat GetFormat() const {
        return format;
    }

    /// Returns the Vulkan aspect mask.
    VkImageAspectFlags GetAspectMask() const {
        return aspect_mask;
    }

private:
    struct SubrangeState final {
        VkAccessFlags access = 0;                         ///< Current access bits.
        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED; ///< Current image layout.
    };

    bool HasChanged(u32 base_layer, u32 num_layers, u32 base_level, u32 num_levels,
                    VkAccessFlags new_access, VkImageLayout new_layout) noexcept;

    /// Creates a presentation view.
    void CreatePresentView();

    /// Returns the subrange state for a layer and layer.
    SubrangeState& GetSubrangeState(u32 layer, u32 level) noexcept;

    const VKDevice& device; ///< Device handler.
    VKScheduler& scheduler; ///< Device scheduler.

    const VkFormat format;                ///< Vulkan format.
    const VkImageAspectFlags aspect_mask; ///< Vulkan aspect mask.
    const u32 image_num_layers;           ///< Number of layers.
    const u32 image_num_levels;           ///< Number of mipmap levels.

    vk::Image image;            ///< Image handle.
    vk::ImageView present_view; ///< Image view compatible with presentation.

    std::vector<VkImageMemoryBarrier> barriers; ///< Pool of barriers.
    std::vector<SubrangeState> subrange_states; ///< Current subrange state.

    bool state_diverged = false; ///< True when subresources mismatch in layout.
};

} // namespace Vulkan
