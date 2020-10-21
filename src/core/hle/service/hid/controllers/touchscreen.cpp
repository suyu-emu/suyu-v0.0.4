// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include "common/common_types.h"
#include "core/core_timing.h"
#include "core/frontend/emu_window.h"
#include "core/frontend/input.h"
#include "core/hle/service/hid/controllers/touchscreen.h"
#include "core/settings.h"

namespace Service::HID {
constexpr std::size_t SHARED_MEMORY_OFFSET = 0x400;

Controller_Touchscreen::Controller_Touchscreen(Core::System& system) : ControllerBase(system) {}
Controller_Touchscreen::~Controller_Touchscreen() = default;

void Controller_Touchscreen::OnInit() {}

void Controller_Touchscreen::OnRelease() {}

void Controller_Touchscreen::OnUpdate(const Core::Timing::CoreTiming& core_timing, u8* data,
                                      std::size_t size) {
    shared_memory.header.timestamp = core_timing.GetCPUTicks();
    shared_memory.header.total_entry_count = 17;

    if (!IsControllerActivated()) {
        shared_memory.header.entry_count = 0;
        shared_memory.header.last_entry_index = 0;
        return;
    }
    shared_memory.header.entry_count = 16;

    const auto& last_entry =
        shared_memory.shared_memory_entries[shared_memory.header.last_entry_index];
    shared_memory.header.last_entry_index = (shared_memory.header.last_entry_index + 1) % 17;
    auto& cur_entry = shared_memory.shared_memory_entries[shared_memory.header.last_entry_index];

    cur_entry.sampling_number = last_entry.sampling_number + 1;
    cur_entry.sampling_number2 = cur_entry.sampling_number;

    bool pressed = false;
    float x, y;
    std::tie(x, y, pressed) = touch_device->GetStatus();
    auto& touch_entry = cur_entry.states[0];
    touch_entry.attribute.raw = 0;
    if (!pressed && touch_btn_device) {
        std::tie(x, y, pressed) = touch_btn_device->GetStatus();
    }
    if (pressed && Settings::values.touchscreen.enabled) {
        touch_entry.x = static_cast<u16>(x * Layout::ScreenUndocked::Width);
        touch_entry.y = static_cast<u16>(y * Layout::ScreenUndocked::Height);
        touch_entry.diameter_x = Settings::values.touchscreen.diameter_x;
        touch_entry.diameter_y = Settings::values.touchscreen.diameter_y;
        touch_entry.rotation_angle = Settings::values.touchscreen.rotation_angle;
        const u64 tick = core_timing.GetCPUTicks();
        touch_entry.delta_time = tick - last_touch;
        last_touch = tick;
        touch_entry.finger = Settings::values.touchscreen.finger;
        cur_entry.entry_count = 1;
    } else {
        cur_entry.entry_count = 0;
    }

    std::memcpy(data + SHARED_MEMORY_OFFSET, &shared_memory, sizeof(TouchScreenSharedMemory));
}

void Controller_Touchscreen::OnLoadInputDevices() {
    touch_device = Input::CreateDevice<Input::TouchDevice>(Settings::values.touchscreen.device);
    if (Settings::values.use_touch_from_button) {
        touch_btn_device = Input::CreateDevice<Input::TouchDevice>("engine:touch_from_button");
    } else {
        touch_btn_device.reset();
    }
}
} // namespace Service::HID
