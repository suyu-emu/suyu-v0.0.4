// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <vector>

#include "common/common_types.h"
#include "video_core/renderer_vulkan/declarations.h"

namespace Vulkan {

class VKDevice;
class VKScheduler;

class VKImage {
public:
    explicit VKImage(const VKDevice& device, VKScheduler& scheduler,
                     const vk::ImageCreateInfo& image_ci, vk::ImageAspectFlags aspect_mask);
    ~VKImage();

    /// Records in the passed command buffer an image transition and updates the state of the image.
    void Transition(u32 base_layer, u32 num_layers, u32 base_level, u32 num_levels,
                    vk::PipelineStageFlags new_stage_mask, vk::AccessFlags new_access,
                    vk::ImageLayout new_layout);

    /// Returns a view compatible with presentation, the image has to be 2D.
    vk::ImageView GetPresentView() {
        if (!present_view) {
            CreatePresentView();
        }
        return *present_view;
    }

    /// Returns the Vulkan image handler.
    vk::Image GetHandle() const {
        return *image;
    }

    /// Returns the Vulkan format for this image.
    vk::Format GetFormat() const {
        return format;
    }

    /// Returns the Vulkan aspect mask.
    vk::ImageAspectFlags GetAspectMask() const {
        return aspect_mask;
    }

private:
    struct SubrangeState final {
        vk::AccessFlags access{};                             ///< Current access bits.
        vk::ImageLayout layout = vk::ImageLayout::eUndefined; ///< Current image layout.
    };

    bool HasChanged(u32 base_layer, u32 num_layers, u32 base_level, u32 num_levels,
                    vk::AccessFlags new_access, vk::ImageLayout new_layout) noexcept;

    /// Creates a presentation view.
    void CreatePresentView();

    /// Returns the subrange state for a layer and layer.
    SubrangeState& GetSubrangeState(u32 layer, u32 level) noexcept;

    const VKDevice& device; ///< Device handler.
    VKScheduler& scheduler; ///< Device scheduler.

    const vk::Format format;                ///< Vulkan format.
    const vk::ImageAspectFlags aspect_mask; ///< Vulkan aspect mask.
    const u32 image_num_layers;             ///< Number of layers.
    const u32 image_num_levels;             ///< Number of mipmap levels.

    UniqueImage image;            ///< Image handle.
    UniqueImageView present_view; ///< Image view compatible with presentation.

    std::vector<vk::ImageMemoryBarrier> barriers; ///< Pool of barriers.
    std::vector<SubrangeState> subrange_states;   ///< Current subrange state.

    bool state_diverged = false; ///< True when subresources mismatch in layout.
};

} // namespace Vulkan
