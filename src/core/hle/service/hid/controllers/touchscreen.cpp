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

    updateTouchInputEvent(touch_mouse_device->GetStatus(), mouse_finger_id);
    updateTouchInputEvent(touch_btn_device->GetStatus(), keyboard_finger_id);
    updateTouchInputEvent(touch_udp_device->GetStatus(), udp_finger_id);

    std::array<Finger, 16> sorted_fingers;
    size_t active_fingers = 0;
    for (Finger finger : fingers) {
        if (finger.pressed) {
            sorted_fingers[active_fingers++] = finger;
        }
    }

    const u64 tick = core_timing.GetCPUTicks();
    cur_entry.entry_count = static_cast<s32_le>(active_fingers);
    for (size_t id = 0; id < MAX_FINGERS; id++) {
        auto& touch_entry = cur_entry.states[id];
        if (id < active_fingers) {
            touch_entry.x = static_cast<u16>(sorted_fingers[id].x * Layout::ScreenUndocked::Width);
            touch_entry.y = static_cast<u16>(sorted_fingers[id].y * Layout::ScreenUndocked::Height);
            touch_entry.diameter_x = Settings::values.touchscreen.diameter_x;
            touch_entry.diameter_y = Settings::values.touchscreen.diameter_y;
            touch_entry.rotation_angle = Settings::values.touchscreen.rotation_angle;
            touch_entry.delta_time = tick - sorted_fingers[id].last_touch;
            sorted_fingers[id].last_touch = tick;
            touch_entry.finger = sorted_fingers[id].id;
            touch_entry.attribute.raw = sorted_fingers[id].attribute.raw;
        } else {
            // Clear touch entry
            touch_entry.attribute.raw = 0;
            touch_entry.x = 0;
            touch_entry.y = 0;
            touch_entry.diameter_x = 0;
            touch_entry.diameter_y = 0;
            touch_entry.rotation_angle = 0;
            touch_entry.delta_time = 0;
            touch_entry.finger = 0;
        }
    }
    std::memcpy(data + SHARED_MEMORY_OFFSET, &shared_memory, sizeof(TouchScreenSharedMemory));
}

void Controller_Touchscreen::OnLoadInputDevices() {
    touch_mouse_device = Input::CreateDevice<Input::TouchDevice>("engine:emu_window");
    touch_udp_device = Input::CreateDevice<Input::TouchDevice>("engine:cemuhookudp");
    if (Settings::values.use_touch_from_button) {
        touch_btn_device = Input::CreateDevice<Input::TouchDevice>("engine:touch_from_button");
    } else {
        touch_btn_device.reset();
    }
}

void Controller_Touchscreen::updateTouchInputEvent(
    const std::tuple<float, float, bool>& touch_input, size_t& finger_id) {
    bool pressed = false;
    float x, y;
    std::tie(x, y, pressed) = touch_input;
    if (pressed) {
        if (finger_id == -1) {
            int first_free_id = 0;
            int found = false;
            while (!found && first_free_id < MAX_FINGERS) {
                if (!fingers[first_free_id].pressed) {
                    found = true;
                } else {
                    first_free_id++;
                }
            }
            if (found) {
                finger_id = first_free_id;
                fingers[finger_id].x = x;
                fingers[finger_id].y = y;
                fingers[finger_id].pressed = true;
                fingers[finger_id].id = static_cast<u32_le>(finger_id);
                fingers[finger_id].attribute.start_touch.Assign(1);
            }
        } else {
            fingers[finger_id].x = x;
            fingers[finger_id].y = y;
            fingers[finger_id].attribute.raw = 0;
        }
    } else if (finger_id != -1) {
        if (!fingers[finger_id].attribute.end_touch) {
            fingers[finger_id].attribute.end_touch.Assign(1);
            fingers[finger_id].attribute.start_touch.Assign(0);
        } else {
            fingers[finger_id].pressed = false;
            finger_id = -1;
        }
    }
}

} // namespace Service::HID
