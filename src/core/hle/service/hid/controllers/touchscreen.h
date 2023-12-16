// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>

#include "core/hid/hid_types.h"
#include "core/hle/service/hid/controllers/controller_base.h"
#include "core/hle/service/hid/controllers/types/touch_types.h"

namespace Core::HID {
class EmulatedConsole;
} // namespace Core::HID

namespace Service::HID {
struct TouchScreenSharedMemoryFormat;

class TouchScreen final : public ControllerBase {
public:
    explicit TouchScreen(Core::HID::HIDCore& hid_core_,
                         TouchScreenSharedMemoryFormat& touch_shared_memory);
    ~TouchScreen() override;

    // Called when the controller is initialized
    void OnInit() override;

    // When the controller is released
    void OnRelease() override;

    // When the controller is requesting an update for the shared memory
    void OnUpdate(const Core::Timing::CoreTiming& core_timing) override;

    void SetTouchscreenDimensions(u32 width, u32 height);

private:
    TouchScreenState next_state{};
    TouchScreenSharedMemoryFormat& shared_memory;
    Core::HID::EmulatedConsole* console = nullptr;

    std::array<Core::HID::TouchFinger, MAX_FINGERS> fingers{};
    u32 touchscreen_width;
    u32 touchscreen_height;
};
} // namespace Service::HID
