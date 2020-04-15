// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <tuple>
#include <vector>

#include "common/assert.h"
#include "common/common_types.h"
#include "common/math_util.h"

#include "core/core.h"
#include "core/frontend/emu_window.h"
#include "core/memory.h"

#include "video_core/gpu.h"
#include "video_core/morton.h"
#include "video_core/rasterizer_interface.h"
#include "video_core/renderer_vulkan/renderer_vulkan.h"
#include "video_core/renderer_vulkan/vk_blit_screen.h"
#include "video_core/renderer_vulkan/vk_device.h"
#include "video_core/renderer_vulkan/vk_image.h"
#include "video_core/renderer_vulkan/vk_memory_manager.h"
#include "video_core/renderer_vulkan/vk_resource_manager.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_shader_util.h"
#include "video_core/renderer_vulkan/vk_swapchain.h"
#include "video_core/renderer_vulkan/wrapper.h"
#include "video_core/surface.h"

namespace Vulkan {

namespace {

// Generated from the "shaders/" directory, read the instructions there.
constexpr u8 blit_vertex_code[] = {
    0x03, 0x02, 0x23, 0x07, 0x00, 0x00, 0x01, 0x00, 0x07, 0x00, 0x08, 0x00, 0x27, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x02, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x06, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x47, 0x4c, 0x53, 0x4c, 0x2e, 0x73, 0x74, 0x64, 0x2e, 0x34, 0x35, 0x30,
    0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x0f, 0x00, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x6d, 0x61, 0x69, 0x6e,
    0x00, 0x00, 0x00, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x19, 0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00,
    0x25, 0x00, 0x00, 0x00, 0x48, 0x00, 0x05, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x48, 0x00, 0x05, 0x00, 0x0b, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x48, 0x00, 0x05, 0x00,
    0x0b, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
    0x48, 0x00, 0x05, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x00, 0x00, 0x47, 0x00, 0x03, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
    0x48, 0x00, 0x04, 0x00, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00,
    0x48, 0x00, 0x05, 0x00, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x23, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x48, 0x00, 0x05, 0x00, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x07, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x47, 0x00, 0x03, 0x00, 0x11, 0x00, 0x00, 0x00,
    0x02, 0x00, 0x00, 0x00, 0x47, 0x00, 0x04, 0x00, 0x13, 0x00, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x47, 0x00, 0x04, 0x00, 0x13, 0x00, 0x00, 0x00, 0x21, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x47, 0x00, 0x04, 0x00, 0x19, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x47, 0x00, 0x04, 0x00, 0x24, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x47, 0x00, 0x04, 0x00, 0x25, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x13, 0x00, 0x02, 0x00, 0x02, 0x00, 0x00, 0x00, 0x21, 0x00, 0x03, 0x00,
    0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x16, 0x00, 0x03, 0x00, 0x06, 0x00, 0x00, 0x00,
    0x20, 0x00, 0x00, 0x00, 0x17, 0x00, 0x04, 0x00, 0x07, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x00, 0x00, 0x15, 0x00, 0x04, 0x00, 0x08, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x2b, 0x00, 0x04, 0x00, 0x08, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x04, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00,
    0x09, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x06, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00,
    0x0c, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x3b, 0x00, 0x04, 0x00,
    0x0c, 0x00, 0x00, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x15, 0x00, 0x04, 0x00,
    0x0e, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x2b, 0x00, 0x04, 0x00,
    0x0e, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x04, 0x00,
    0x10, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x03, 0x00,
    0x11, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 0x12, 0x00, 0x00, 0x00,
    0x02, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x3b, 0x00, 0x04, 0x00, 0x12, 0x00, 0x00, 0x00,
    0x13, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 0x14, 0x00, 0x00, 0x00,
    0x02, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x17, 0x00, 0x04, 0x00, 0x17, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 0x18, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x17, 0x00, 0x00, 0x00, 0x3b, 0x00, 0x04, 0x00, 0x18, 0x00, 0x00, 0x00,
    0x19, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x2b, 0x00, 0x04, 0x00, 0x06, 0x00, 0x00, 0x00,
    0x1b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2b, 0x00, 0x04, 0x00, 0x06, 0x00, 0x00, 0x00,
    0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3f, 0x20, 0x00, 0x04, 0x00, 0x21, 0x00, 0x00, 0x00,
    0x03, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 0x23, 0x00, 0x00, 0x00,
    0x03, 0x00, 0x00, 0x00, 0x17, 0x00, 0x00, 0x00, 0x3b, 0x00, 0x04, 0x00, 0x23, 0x00, 0x00, 0x00,
    0x24, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x3b, 0x00, 0x04, 0x00, 0x18, 0x00, 0x00, 0x00,
    0x25, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x36, 0x00, 0x05, 0x00, 0x02, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0xf8, 0x00, 0x02, 0x00,
    0x05, 0x00, 0x00, 0x00, 0x41, 0x00, 0x05, 0x00, 0x14, 0x00, 0x00, 0x00, 0x15, 0x00, 0x00, 0x00,
    0x13, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x3d, 0x00, 0x04, 0x00, 0x10, 0x00, 0x00, 0x00,
    0x16, 0x00, 0x00, 0x00, 0x15, 0x00, 0x00, 0x00, 0x3d, 0x00, 0x04, 0x00, 0x17, 0x00, 0x00, 0x00,
    0x1a, 0x00, 0x00, 0x00, 0x19, 0x00, 0x00, 0x00, 0x51, 0x00, 0x05, 0x00, 0x06, 0x00, 0x00, 0x00,
    0x1d, 0x00, 0x00, 0x00, 0x1a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x51, 0x00, 0x05, 0x00,
    0x06, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, 0x1a, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x50, 0x00, 0x07, 0x00, 0x07, 0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00, 0x1d, 0x00, 0x00, 0x00,
    0x1e, 0x00, 0x00, 0x00, 0x1b, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x91, 0x00, 0x05, 0x00,
    0x07, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x16, 0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00,
    0x41, 0x00, 0x05, 0x00, 0x21, 0x00, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00, 0x0d, 0x00, 0x00, 0x00,
    0x0f, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x03, 0x00, 0x22, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00,
    0x3d, 0x00, 0x04, 0x00, 0x17, 0x00, 0x00, 0x00, 0x26, 0x00, 0x00, 0x00, 0x25, 0x00, 0x00, 0x00,
    0x3e, 0x00, 0x03, 0x00, 0x24, 0x00, 0x00, 0x00, 0x26, 0x00, 0x00, 0x00, 0xfd, 0x00, 0x01, 0x00,
    0x38, 0x00, 0x01, 0x00};

constexpr u8 blit_fragment_code[] = {
    0x03, 0x02, 0x23, 0x07, 0x00, 0x00, 0x01, 0x00, 0x07, 0x00, 0x08, 0x00, 0x14, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x02, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x06, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x47, 0x4c, 0x53, 0x4c, 0x2e, 0x73, 0x74, 0x64, 0x2e, 0x34, 0x35, 0x30,
    0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x0f, 0x00, 0x07, 0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x6d, 0x61, 0x69, 0x6e,
    0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x10, 0x00, 0x03, 0x00,
    0x04, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x47, 0x00, 0x04, 0x00, 0x09, 0x00, 0x00, 0x00,
    0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x47, 0x00, 0x04, 0x00, 0x0d, 0x00, 0x00, 0x00,
    0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x47, 0x00, 0x04, 0x00, 0x0d, 0x00, 0x00, 0x00,
    0x21, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x47, 0x00, 0x04, 0x00, 0x11, 0x00, 0x00, 0x00,
    0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x13, 0x00, 0x02, 0x00, 0x02, 0x00, 0x00, 0x00,
    0x21, 0x00, 0x03, 0x00, 0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x16, 0x00, 0x03, 0x00,
    0x06, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x17, 0x00, 0x04, 0x00, 0x07, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 0x08, 0x00, 0x00, 0x00,
    0x03, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x3b, 0x00, 0x04, 0x00, 0x08, 0x00, 0x00, 0x00,
    0x09, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x19, 0x00, 0x09, 0x00, 0x0a, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1b, 0x00, 0x03, 0x00,
    0x0b, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 0x0c, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x3b, 0x00, 0x04, 0x00, 0x0c, 0x00, 0x00, 0x00,
    0x0d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x17, 0x00, 0x04, 0x00, 0x0f, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 0x10, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x3b, 0x00, 0x04, 0x00, 0x10, 0x00, 0x00, 0x00,
    0x11, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x36, 0x00, 0x05, 0x00, 0x02, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0xf8, 0x00, 0x02, 0x00,
    0x05, 0x00, 0x00, 0x00, 0x3d, 0x00, 0x04, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00,
    0x0d, 0x00, 0x00, 0x00, 0x3d, 0x00, 0x04, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00,
    0x11, 0x00, 0x00, 0x00, 0x57, 0x00, 0x05, 0x00, 0x07, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00,
    0x0e, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x03, 0x00, 0x09, 0x00, 0x00, 0x00,
    0x13, 0x00, 0x00, 0x00, 0xfd, 0x00, 0x01, 0x00, 0x38, 0x00, 0x01, 0x00};

struct ScreenRectVertex {
    ScreenRectVertex() = default;
    explicit ScreenRectVertex(f32 x, f32 y, f32 u, f32 v) : position{{x, y}}, tex_coord{{u, v}} {}

    std::array<f32, 2> position;
    std::array<f32, 2> tex_coord;

    static VkVertexInputBindingDescription GetDescription() {
        VkVertexInputBindingDescription description;
        description.binding = 0;
        description.stride = sizeof(ScreenRectVertex);
        description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return description;
    }

    static std::array<VkVertexInputAttributeDescription, 2> GetAttributes() {
        std::array<VkVertexInputAttributeDescription, 2> attributes;
        attributes[0].location = 0;
        attributes[0].binding = 0;
        attributes[0].format = VK_FORMAT_R32G32_SFLOAT;
        attributes[0].offset = offsetof(ScreenRectVertex, position);
        attributes[1].location = 1;
        attributes[1].binding = 0;
        attributes[1].format = VK_FORMAT_R32G32_SFLOAT;
        attributes[1].offset = offsetof(ScreenRectVertex, tex_coord);
        return attributes;
    }
};

constexpr std::array<f32, 4 * 4> MakeOrthographicMatrix(f32 width, f32 height) {
    // clang-format off
    return { 2.f / width, 0.f,          0.f, 0.f,
             0.f,         2.f / height, 0.f, 0.f,
             0.f,         0.f,          1.f, 0.f,
            -1.f,        -1.f,          0.f, 1.f};
    // clang-format on
}

std::size_t GetBytesPerPixel(const Tegra::FramebufferConfig& framebuffer) {
    using namespace VideoCore::Surface;
    return GetBytesPerPixel(PixelFormatFromGPUPixelFormat(framebuffer.pixel_format));
}

std::size_t GetSizeInBytes(const Tegra::FramebufferConfig& framebuffer) {
    return static_cast<std::size_t>(framebuffer.stride) *
           static_cast<std::size_t>(framebuffer.height) * GetBytesPerPixel(framebuffer);
}

VkFormat GetFormat(const Tegra::FramebufferConfig& framebuffer) {
    switch (framebuffer.pixel_format) {
    case Tegra::FramebufferConfig::PixelFormat::ABGR8:
        return VK_FORMAT_A8B8G8R8_UNORM_PACK32;
    case Tegra::FramebufferConfig::PixelFormat::RGB565:
        return VK_FORMAT_R5G6B5_UNORM_PACK16;
    default:
        UNIMPLEMENTED_MSG("Unknown framebuffer pixel format: {}",
                          static_cast<u32>(framebuffer.pixel_format));
        return VK_FORMAT_A8B8G8R8_UNORM_PACK32;
    }
}

} // Anonymous namespace

struct VKBlitScreen::BufferData {
    struct {
        std::array<f32, 4 * 4> modelview_matrix;
    } uniform;

    std::array<ScreenRectVertex, 4> vertices;

    // Unaligned image data goes here
};

VKBlitScreen::VKBlitScreen(Core::System& system, Core::Frontend::EmuWindow& render_window,
                           VideoCore::RasterizerInterface& rasterizer, const VKDevice& device,
                           VKResourceManager& resource_manager, VKMemoryManager& memory_manager,
                           VKSwapchain& swapchain, VKScheduler& scheduler,
                           const VKScreenInfo& screen_info)
    : system{system}, render_window{render_window}, rasterizer{rasterizer}, device{device},
      resource_manager{resource_manager}, memory_manager{memory_manager}, swapchain{swapchain},
      scheduler{scheduler}, image_count{swapchain.GetImageCount()}, screen_info{screen_info} {
    watches.resize(image_count);
    std::generate(watches.begin(), watches.end(),
                  []() { return std::make_unique<VKFenceWatch>(); });

    CreateStaticResources();
    CreateDynamicResources();
}

VKBlitScreen::~VKBlitScreen() = default;

void VKBlitScreen::Recreate() {
    CreateDynamicResources();
}

std::tuple<VKFence&, VkSemaphore> VKBlitScreen::Draw(const Tegra::FramebufferConfig& framebuffer,
                                                     bool use_accelerated) {
    RefreshResources(framebuffer);

    // Finish any pending renderpass
    scheduler.RequestOutsideRenderPassOperationContext();

    const std::size_t image_index = swapchain.GetImageIndex();
    watches[image_index]->Watch(scheduler.GetFence());

    VKImage* blit_image = use_accelerated ? screen_info.image : raw_images[image_index].get();

    UpdateDescriptorSet(image_index, blit_image->GetPresentView());

    BufferData data;
    SetUniformData(data, framebuffer);
    SetVertexData(data, framebuffer);

    auto map = buffer_commit->Map();
    std::memcpy(map.GetAddress(), &data, sizeof(data));

    if (!use_accelerated) {
        const u64 image_offset = GetRawImageOffset(framebuffer, image_index);

        const auto pixel_format =
            VideoCore::Surface::PixelFormatFromGPUPixelFormat(framebuffer.pixel_format);
        const VAddr framebuffer_addr = framebuffer.address + framebuffer.offset;
        const auto host_ptr = system.Memory().GetPointer(framebuffer_addr);
        rasterizer.FlushRegion(ToCacheAddr(host_ptr), GetSizeInBytes(framebuffer));

        // TODO(Rodrigo): Read this from HLE
        constexpr u32 block_height_log2 = 4;
        VideoCore::MortonSwizzle(VideoCore::MortonSwizzleMode::MortonToLinear, pixel_format,
                                 framebuffer.stride, block_height_log2, framebuffer.height, 0, 1, 1,
                                 map.GetAddress() + image_offset, host_ptr);

        blit_image->Transition(0, 1, 0, 1, VK_PIPELINE_STAGE_TRANSFER_BIT,
                               VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkBufferImageCopy copy;
        copy.bufferOffset = image_offset;
        copy.bufferRowLength = 0;
        copy.bufferImageHeight = 0;
        copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy.imageSubresource.mipLevel = 0;
        copy.imageSubresource.baseArrayLayer = 0;
        copy.imageSubresource.layerCount = 1;
        copy.imageOffset.x = 0;
        copy.imageOffset.y = 0;
        copy.imageOffset.z = 0;
        copy.imageExtent.width = framebuffer.width;
        copy.imageExtent.height = framebuffer.height;
        copy.imageExtent.depth = 1;
        scheduler.Record(
            [buffer = *buffer, image = *blit_image->GetHandle(), copy](vk::CommandBuffer cmdbuf) {
                cmdbuf.CopyBufferToImage(buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, copy);
            });
    }
    map.Release();

    blit_image->Transition(0, 1, 0, 1, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                           VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    scheduler.Record([renderpass = *renderpass, framebuffer = *framebuffers[image_index],
                      descriptor_set = descriptor_sets[image_index], buffer = *buffer,
                      size = swapchain.GetSize(), pipeline = *pipeline,
                      layout = *pipeline_layout](vk::CommandBuffer cmdbuf) {
        VkClearValue clear_color;
        clear_color.color.float32[0] = 0.0f;
        clear_color.color.float32[1] = 0.0f;
        clear_color.color.float32[2] = 0.0f;
        clear_color.color.float32[3] = 0.0f;

        VkRenderPassBeginInfo renderpass_bi;
        renderpass_bi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderpass_bi.pNext = nullptr;
        renderpass_bi.renderPass = renderpass;
        renderpass_bi.framebuffer = framebuffer;
        renderpass_bi.renderArea.offset.x = 0;
        renderpass_bi.renderArea.offset.y = 0;
        renderpass_bi.renderArea.extent = size;
        renderpass_bi.clearValueCount = 1;
        renderpass_bi.pClearValues = &clear_color;

        VkViewport viewport;
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(size.width);
        viewport.height = static_cast<float>(size.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor;
        scissor.offset.x = 0;
        scissor.offset.y = 0;
        scissor.extent = size;

        cmdbuf.BeginRenderPass(renderpass_bi, VK_SUBPASS_CONTENTS_INLINE);
        cmdbuf.BindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        cmdbuf.SetViewport(0, viewport);
        cmdbuf.SetScissor(0, scissor);

        cmdbuf.BindVertexBuffer(0, buffer, offsetof(BufferData, vertices));
        cmdbuf.BindDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, descriptor_set, {});
        cmdbuf.Draw(4, 1, 0, 0);
        cmdbuf.EndRenderPass();
    });

    return {scheduler.GetFence(), *semaphores[image_index]};
}

void VKBlitScreen::CreateStaticResources() {
    CreateShaders();
    CreateSemaphores();
    CreateDescriptorPool();
    CreateDescriptorSetLayout();
    CreateDescriptorSets();
    CreatePipelineLayout();
    CreateSampler();
}

void VKBlitScreen::CreateDynamicResources() {
    CreateRenderPass();
    CreateFramebuffers();
    CreateGraphicsPipeline();
}

void VKBlitScreen::RefreshResources(const Tegra::FramebufferConfig& framebuffer) {
    if (framebuffer.width == raw_width && framebuffer.height == raw_height && !raw_images.empty()) {
        return;
    }
    raw_width = framebuffer.width;
    raw_height = framebuffer.height;
    ReleaseRawImages();

    CreateStagingBuffer(framebuffer);
    CreateRawImages(framebuffer);
}

void VKBlitScreen::CreateShaders() {
    vertex_shader = BuildShader(device, sizeof(blit_vertex_code), blit_vertex_code);
    fragment_shader = BuildShader(device, sizeof(blit_fragment_code), blit_fragment_code);
}

void VKBlitScreen::CreateSemaphores() {
    semaphores.resize(image_count);
    std::generate(semaphores.begin(), semaphores.end(),
                  [this] { return device.GetLogical().CreateSemaphore(); });
}

void VKBlitScreen::CreateDescriptorPool() {
    std::array<VkDescriptorPoolSize, 2> pool_sizes;
    pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_sizes[0].descriptorCount = static_cast<u32>(image_count);
    pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_sizes[1].descriptorCount = static_cast<u32>(image_count);

    VkDescriptorPoolCreateInfo ci;
    ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    ci.pNext = nullptr;
    ci.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    ci.maxSets = static_cast<u32>(image_count);
    ci.poolSizeCount = static_cast<u32>(pool_sizes.size());
    ci.pPoolSizes = pool_sizes.data();
    descriptor_pool = device.GetLogical().CreateDescriptorPool(ci);
}

void VKBlitScreen::CreateRenderPass() {
    VkAttachmentDescription color_attachment;
    color_attachment.flags = 0;
    color_attachment.format = swapchain.GetImageFormat();
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_ref;
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass_description;
    subpass_description.flags = 0;
    subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_description.inputAttachmentCount = 0;
    subpass_description.pInputAttachments = nullptr;
    subpass_description.colorAttachmentCount = 1;
    subpass_description.pColorAttachments = &color_attachment_ref;
    subpass_description.pResolveAttachments = nullptr;
    subpass_description.pDepthStencilAttachment = nullptr;
    subpass_description.preserveAttachmentCount = 0;
    subpass_description.pPreserveAttachments = nullptr;

    VkSubpassDependency dependency;
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependency.dependencyFlags = 0;

    VkRenderPassCreateInfo renderpass_ci;
    renderpass_ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderpass_ci.pNext = nullptr;
    renderpass_ci.flags = 0;
    renderpass_ci.attachmentCount = 1;
    renderpass_ci.pAttachments = &color_attachment;
    renderpass_ci.subpassCount = 1;
    renderpass_ci.pSubpasses = &subpass_description;
    renderpass_ci.dependencyCount = 1;
    renderpass_ci.pDependencies = &dependency;

    renderpass = device.GetLogical().CreateRenderPass(renderpass_ci);
}

void VKBlitScreen::CreateDescriptorSetLayout() {
    std::array<VkDescriptorSetLayoutBinding, 2> layout_bindings;
    layout_bindings[0].binding = 0;
    layout_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layout_bindings[0].descriptorCount = 1;
    layout_bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    layout_bindings[0].pImmutableSamplers = nullptr;
    layout_bindings[1].binding = 1;
    layout_bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layout_bindings[1].descriptorCount = 1;
    layout_bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layout_bindings[1].pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo ci;
    ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ci.pNext = nullptr;
    ci.flags = 0;
    ci.bindingCount = static_cast<u32>(layout_bindings.size());
    ci.pBindings = layout_bindings.data();

    descriptor_set_layout = device.GetLogical().CreateDescriptorSetLayout(ci);
}

void VKBlitScreen::CreateDescriptorSets() {
    const std::vector layouts(image_count, *descriptor_set_layout);

    VkDescriptorSetAllocateInfo ai;
    ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.pNext = nullptr;
    ai.descriptorPool = *descriptor_pool;
    ai.descriptorSetCount = static_cast<u32>(image_count);
    ai.pSetLayouts = layouts.data();
    descriptor_sets = descriptor_pool.Allocate(ai);
}

void VKBlitScreen::CreatePipelineLayout() {
    VkPipelineLayoutCreateInfo ci;
    ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    ci.pNext = nullptr;
    ci.flags = 0;
    ci.setLayoutCount = 1;
    ci.pSetLayouts = descriptor_set_layout.address();
    ci.pushConstantRangeCount = 0;
    ci.pPushConstantRanges = nullptr;
    pipeline_layout = device.GetLogical().CreatePipelineLayout(ci);
}

void VKBlitScreen::CreateGraphicsPipeline() {
    std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages;
    shader_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stages[0].pNext = nullptr;
    shader_stages[0].flags = 0;
    shader_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shader_stages[0].module = *vertex_shader;
    shader_stages[0].pName = "main";
    shader_stages[0].pSpecializationInfo = nullptr;
    shader_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stages[1].pNext = nullptr;
    shader_stages[1].flags = 0;
    shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shader_stages[1].module = *fragment_shader;
    shader_stages[1].pName = "main";
    shader_stages[1].pSpecializationInfo = nullptr;

    const auto vertex_binding_description = ScreenRectVertex::GetDescription();
    const auto vertex_attrs_description = ScreenRectVertex::GetAttributes();

    VkPipelineVertexInputStateCreateInfo vertex_input_ci;
    vertex_input_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_ci.pNext = nullptr;
    vertex_input_ci.flags = 0;
    vertex_input_ci.vertexBindingDescriptionCount = 1;
    vertex_input_ci.pVertexBindingDescriptions = &vertex_binding_description;
    vertex_input_ci.vertexAttributeDescriptionCount = u32{vertex_attrs_description.size()};
    vertex_input_ci.pVertexAttributeDescriptions = vertex_attrs_description.data();

    VkPipelineInputAssemblyStateCreateInfo input_assembly_ci;
    input_assembly_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_ci.pNext = nullptr;
    input_assembly_ci.flags = 0;
    input_assembly_ci.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    input_assembly_ci.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewport_state_ci;
    viewport_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state_ci.pNext = nullptr;
    viewport_state_ci.flags = 0;
    viewport_state_ci.viewportCount = 1;
    viewport_state_ci.pViewports = nullptr;
    viewport_state_ci.scissorCount = 1;
    viewport_state_ci.pScissors = nullptr;

    VkPipelineRasterizationStateCreateInfo rasterization_ci;
    rasterization_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization_ci.pNext = nullptr;
    rasterization_ci.flags = 0;
    rasterization_ci.depthClampEnable = VK_FALSE;
    rasterization_ci.rasterizerDiscardEnable = VK_FALSE;
    rasterization_ci.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization_ci.cullMode = VK_CULL_MODE_NONE;
    rasterization_ci.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterization_ci.depthBiasEnable = VK_FALSE;
    rasterization_ci.depthBiasConstantFactor = 0.0f;
    rasterization_ci.depthBiasClamp = 0.0f;
    rasterization_ci.depthBiasSlopeFactor = 0.0f;
    rasterization_ci.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampling_ci;
    multisampling_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling_ci.pNext = nullptr;
    multisampling_ci.flags = 0;
    multisampling_ci.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling_ci.sampleShadingEnable = VK_FALSE;
    multisampling_ci.minSampleShading = 0.0f;
    multisampling_ci.pSampleMask = nullptr;
    multisampling_ci.alphaToCoverageEnable = VK_FALSE;
    multisampling_ci.alphaToOneEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState color_blend_attachment;
    color_blend_attachment.blendEnable = VK_FALSE;
    color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo color_blend_ci;
    color_blend_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend_ci.flags = 0;
    color_blend_ci.pNext = nullptr;
    color_blend_ci.logicOpEnable = VK_FALSE;
    color_blend_ci.logicOp = VK_LOGIC_OP_COPY;
    color_blend_ci.attachmentCount = 1;
    color_blend_ci.pAttachments = &color_blend_attachment;
    color_blend_ci.blendConstants[0] = 0.0f;
    color_blend_ci.blendConstants[1] = 0.0f;
    color_blend_ci.blendConstants[2] = 0.0f;
    color_blend_ci.blendConstants[3] = 0.0f;

    static constexpr std::array dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT,
                                                  VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic_state_ci;
    dynamic_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state_ci.pNext = nullptr;
    dynamic_state_ci.flags = 0;
    dynamic_state_ci.dynamicStateCount = static_cast<u32>(dynamic_states.size());
    dynamic_state_ci.pDynamicStates = dynamic_states.data();

    VkGraphicsPipelineCreateInfo pipeline_ci;
    pipeline_ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_ci.pNext = nullptr;
    pipeline_ci.flags = 0;
    pipeline_ci.stageCount = static_cast<u32>(shader_stages.size());
    pipeline_ci.pStages = shader_stages.data();
    pipeline_ci.pVertexInputState = &vertex_input_ci;
    pipeline_ci.pInputAssemblyState = &input_assembly_ci;
    pipeline_ci.pTessellationState = nullptr;
    pipeline_ci.pViewportState = &viewport_state_ci;
    pipeline_ci.pRasterizationState = &rasterization_ci;
    pipeline_ci.pMultisampleState = &multisampling_ci;
    pipeline_ci.pDepthStencilState = nullptr;
    pipeline_ci.pColorBlendState = &color_blend_ci;
    pipeline_ci.pDynamicState = &dynamic_state_ci;
    pipeline_ci.layout = *pipeline_layout;
    pipeline_ci.renderPass = *renderpass;
    pipeline_ci.subpass = 0;
    pipeline_ci.basePipelineHandle = 0;
    pipeline_ci.basePipelineIndex = 0;

    pipeline = device.GetLogical().CreateGraphicsPipeline(pipeline_ci);
}

void VKBlitScreen::CreateSampler() {
    VkSamplerCreateInfo ci;
    ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    ci.pNext = nullptr;
    ci.flags = 0;
    ci.magFilter = VK_FILTER_LINEAR;
    ci.minFilter = VK_FILTER_NEAREST;
    ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    ci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    ci.mipLodBias = 0.0f;
    ci.anisotropyEnable = VK_FALSE;
    ci.maxAnisotropy = 0.0f;
    ci.compareEnable = VK_FALSE;
    ci.compareOp = VK_COMPARE_OP_NEVER;
    ci.minLod = 0.0f;
    ci.maxLod = 0.0f;
    ci.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    ci.unnormalizedCoordinates = VK_FALSE;

    sampler = device.GetLogical().CreateSampler(ci);
}

void VKBlitScreen::CreateFramebuffers() {
    const VkExtent2D size{swapchain.GetSize()};
    framebuffers.resize(image_count);

    VkFramebufferCreateInfo ci;
    ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    ci.pNext = nullptr;
    ci.flags = 0;
    ci.renderPass = *renderpass;
    ci.attachmentCount = 1;
    ci.width = size.width;
    ci.height = size.height;
    ci.layers = 1;

    for (std::size_t i = 0; i < image_count; ++i) {
        const VkImageView image_view{swapchain.GetImageViewIndex(i)};
        ci.pAttachments = &image_view;
        framebuffers[i] = device.GetLogical().CreateFramebuffer(ci);
    }
}

void VKBlitScreen::ReleaseRawImages() {
    for (std::size_t i = 0; i < raw_images.size(); ++i) {
        watches[i]->Wait();
    }
    raw_images.clear();
    raw_buffer_commits.clear();
    buffer.reset();
    buffer_commit.reset();
}

void VKBlitScreen::CreateStagingBuffer(const Tegra::FramebufferConfig& framebuffer) {
    VkBufferCreateInfo ci;
    ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    ci.pNext = nullptr;
    ci.flags = 0;
    ci.size = CalculateBufferSize(framebuffer);
    ci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
               VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.queueFamilyIndexCount = 0;
    ci.pQueueFamilyIndices = nullptr;

    buffer = device.GetLogical().CreateBuffer(ci);
    buffer_commit = memory_manager.Commit(buffer, true);
}

void VKBlitScreen::CreateRawImages(const Tegra::FramebufferConfig& framebuffer) {
    raw_images.resize(image_count);
    raw_buffer_commits.resize(image_count);

    VkImageCreateInfo ci;
    ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ci.pNext = nullptr;
    ci.flags = 0;
    ci.imageType = VK_IMAGE_TYPE_2D;
    ci.format = GetFormat(framebuffer);
    ci.extent.width = framebuffer.width;
    ci.extent.height = framebuffer.height;
    ci.extent.depth = 1;
    ci.mipLevels = 1;
    ci.arrayLayers = 1;
    ci.samples = VK_SAMPLE_COUNT_1_BIT;
    ci.tiling = VK_IMAGE_TILING_LINEAR;
    ci.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.queueFamilyIndexCount = 0;
    ci.pQueueFamilyIndices = nullptr;
    ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    for (std::size_t i = 0; i < image_count; ++i) {
        raw_images[i] = std::make_unique<VKImage>(device, scheduler, ci, VK_IMAGE_ASPECT_COLOR_BIT);
        raw_buffer_commits[i] = memory_manager.Commit(raw_images[i]->GetHandle(), false);
    }
}

void VKBlitScreen::UpdateDescriptorSet(std::size_t image_index, VkImageView image_view) const {
    VkDescriptorBufferInfo buffer_info;
    buffer_info.buffer = *buffer;
    buffer_info.offset = offsetof(BufferData, uniform);
    buffer_info.range = sizeof(BufferData::uniform);

    VkWriteDescriptorSet ubo_write;
    ubo_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    ubo_write.pNext = nullptr;
    ubo_write.dstSet = descriptor_sets[image_index];
    ubo_write.dstBinding = 0;
    ubo_write.dstArrayElement = 0;
    ubo_write.descriptorCount = 1;
    ubo_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubo_write.pImageInfo = nullptr;
    ubo_write.pBufferInfo = &buffer_info;
    ubo_write.pTexelBufferView = nullptr;

    VkDescriptorImageInfo image_info;
    image_info.sampler = *sampler;
    image_info.imageView = image_view;
    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet sampler_write;
    sampler_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    sampler_write.pNext = nullptr;
    sampler_write.dstSet = descriptor_sets[image_index];
    sampler_write.dstBinding = 1;
    sampler_write.dstArrayElement = 0;
    sampler_write.descriptorCount = 1;
    sampler_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sampler_write.pImageInfo = &image_info;
    sampler_write.pBufferInfo = nullptr;
    sampler_write.pTexelBufferView = nullptr;

    device.GetLogical().UpdateDescriptorSets(std::array{ubo_write, sampler_write}, {});
}

void VKBlitScreen::SetUniformData(BufferData& data,
                                  const Tegra::FramebufferConfig& framebuffer) const {
    const auto& layout = render_window.GetFramebufferLayout();
    data.uniform.modelview_matrix =
        MakeOrthographicMatrix(static_cast<f32>(layout.width), static_cast<f32>(layout.height));
}

void VKBlitScreen::SetVertexData(BufferData& data,
                                 const Tegra::FramebufferConfig& framebuffer) const {
    const auto& framebuffer_transform_flags = framebuffer.transform_flags;
    const auto& framebuffer_crop_rect = framebuffer.crop_rect;

    static constexpr Common::Rectangle<f32> texcoords{0.f, 0.f, 1.f, 1.f};
    auto left = texcoords.left;
    auto right = texcoords.right;

    switch (framebuffer_transform_flags) {
    case Tegra::FramebufferConfig::TransformFlags::Unset:
        break;
    case Tegra::FramebufferConfig::TransformFlags::FlipV:
        // Flip the framebuffer vertically
        left = texcoords.right;
        right = texcoords.left;
        break;
    default:
        UNIMPLEMENTED_MSG("Unsupported framebuffer_transform_flags={}",
                          static_cast<u32>(framebuffer_transform_flags));
        break;
    }

    UNIMPLEMENTED_IF(framebuffer_crop_rect.top != 0);
    UNIMPLEMENTED_IF(framebuffer_crop_rect.left != 0);

    // Scale the output by the crop width/height. This is commonly used with 1280x720 rendering
    // (e.g. handheld mode) on a 1920x1080 framebuffer.
    f32 scale_u = 1.0f;
    f32 scale_v = 1.0f;
    if (framebuffer_crop_rect.GetWidth() > 0) {
        scale_u = static_cast<f32>(framebuffer_crop_rect.GetWidth()) /
                  static_cast<f32>(screen_info.width);
    }
    if (framebuffer_crop_rect.GetHeight() > 0) {
        scale_v = static_cast<f32>(framebuffer_crop_rect.GetHeight()) /
                  static_cast<f32>(screen_info.height);
    }

    const auto& screen = render_window.GetFramebufferLayout().screen;
    const auto x = static_cast<f32>(screen.left);
    const auto y = static_cast<f32>(screen.top);
    const auto w = static_cast<f32>(screen.GetWidth());
    const auto h = static_cast<f32>(screen.GetHeight());
    data.vertices[0] = ScreenRectVertex(x, y, texcoords.top * scale_u, left * scale_v);
    data.vertices[1] = ScreenRectVertex(x + w, y, texcoords.bottom * scale_u, left * scale_v);
    data.vertices[2] = ScreenRectVertex(x, y + h, texcoords.top * scale_u, right * scale_v);
    data.vertices[3] = ScreenRectVertex(x + w, y + h, texcoords.bottom * scale_u, right * scale_v);
}

u64 VKBlitScreen::CalculateBufferSize(const Tegra::FramebufferConfig& framebuffer) const {
    return sizeof(BufferData) + GetSizeInBytes(framebuffer) * image_count;
}

u64 VKBlitScreen::GetRawImageOffset(const Tegra::FramebufferConfig& framebuffer,
                                    std::size_t image_index) const {
    constexpr auto first_image_offset = static_cast<u64>(sizeof(BufferData));
    return first_image_offset + GetSizeInBytes(framebuffer) * image_index;
}

} // namespace Vulkan
