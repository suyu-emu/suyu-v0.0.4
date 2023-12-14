// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/common_types.h"
#include "core/hle/service/hid/controllers/controller_base.h"
#include "core/hle/service/hid/ring_lifo.h"

namespace Core::HID {
class EmulatedDevices;
struct MouseState;
struct AnalogStickState;
} // namespace Core::HID

namespace Service::HID {
struct MouseSharedMemoryFormat;

class Mouse final : public ControllerBase {
public:
    explicit Mouse(Core::HID::HIDCore& hid_core_, MouseSharedMemoryFormat& mouse_shared_memory);
    ~Mouse() override;

    // Called when the controller is initialized
    void OnInit() override;

    // When the controller is released
    void OnRelease() override;

    // When the controller is requesting an update for the shared memory
    void OnUpdate(const Core::Timing::CoreTiming& core_timing) override;

private:
    Core::HID::MouseState next_state{};
    Core::HID::AnalogStickState last_mouse_wheel_state{};
    MouseSharedMemoryFormat& shared_memory;
    Core::HID::EmulatedDevices* emulated_devices = nullptr;
};
} // namespace Service::HID
