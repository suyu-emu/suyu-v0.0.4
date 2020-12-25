// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <utility>

#include "common/common_types.h"
#include "common/dynamic_library.h"
#include "core/frontend/emu_window.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

[[nodiscard]] std::pair<vk::Instance, u32> CreateInstance(
    Common::DynamicLibrary& library, vk::InstanceDispatch& dld,
    Core::Frontend::WindowSystemType window_type = Core::Frontend::WindowSystemType::Headless,
    bool enable_debug_utils = false, bool enable_layers = false);

} // namespace Vulkan
