// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include "common/common_types.h"
#include "common/settings.h"
#include "core/core_timing.h"
#include "core/frontend/emu_window.h"
#include "core/hid/emulated_console.h"
#include "core/hid/hid_core.h"
#include "core/hle/service/hid/controllers/applet_resource.h"
#include "core/hle/service/hid/controllers/touchscreen.h"
#include "core/hle/service/hid/controllers/types/shared_memory_format.h"

namespace Service::HID {

TouchScreen::TouchScreen(Core::HID::HIDCore& hid_core_)
    : ControllerBase{hid_core_}, touchscreen_width(Layout::ScreenUndocked::Width),
      touchscreen_height(Layout::ScreenUndocked::Height) {
    console = hid_core.GetEmulatedConsole();
}

TouchScreen::~TouchScreen() = default;

void TouchScreen::OnInit() {}

void TouchScreen::OnRelease() {}

void TouchScreen::OnUpdate(const Core::Timing::CoreTiming& core_timing) {
    const u64 aruid = applet_resource->GetActiveAruid();
    auto* data = applet_resource->GetAruidData(aruid);

    if (data == nullptr || !data->flag.is_assigned) {
        return;
    }

    TouchScreenSharedMemoryFormat& shared_memory = data->shared_memory_format->touch_screen;
    shared_memory.touch_screen_lifo.timestamp = core_timing.GetGlobalTimeNs().count();

    if (!IsControllerActivated()) {
        shared_memory.touch_screen_lifo.buffer_count = 0;
        shared_memory.touch_screen_lifo.buffer_tail = 0;
        return;
    }

    const auto touch_status = console->GetTouch();
    for (std::size_t id = 0; id < MAX_FINGERS; id++) {
        const auto& current_touch = touch_status[id];
        auto& finger = fingers[id];
        finger.id = current_touch.id;

        if (finger.attribute.start_touch) {
            finger.attribute.raw = 0;
            continue;
        }

        if (finger.attribute.end_touch) {
            finger.attribute.raw = 0;
            finger.pressed = false;
            continue;
        }

        if (!finger.pressed && current_touch.pressed) {
            // Ignore all touch fingers if disabled
            if (!Settings::values.touchscreen.enabled) {
                continue;
            }

            finger.attribute.start_touch.Assign(1);
            finger.pressed = true;
            finger.position = current_touch.position;
            continue;
        }

        if (finger.pressed && !current_touch.pressed) {
            finger.attribute.raw = 0;
            finger.attribute.end_touch.Assign(1);
            continue;
        }

        // Only update position if touch is not on a special frame
        finger.position = current_touch.position;
    }

    std::array<Core::HID::TouchFinger, MAX_FINGERS> active_fingers;
    const auto end_iter = std::copy_if(fingers.begin(), fingers.end(), active_fingers.begin(),
                                       [](const auto& finger) { return finger.pressed; });
    const auto active_fingers_count =
        static_cast<std::size_t>(std::distance(active_fingers.begin(), end_iter));

    const u64 timestamp = static_cast<u64>(core_timing.GetGlobalTimeNs().count());
    const auto& last_entry = shared_memory.touch_screen_lifo.ReadCurrentEntry().state;

    next_state.sampling_number = last_entry.sampling_number + 1;
    next_state.entry_count = static_cast<s32>(active_fingers_count);

    for (std::size_t id = 0; id < MAX_FINGERS; ++id) {
        auto& touch_entry = next_state.states[id];
        if (id < active_fingers_count) {
            const auto& [active_x, active_y] = active_fingers[id].position;
            touch_entry.position = {
                .x = static_cast<u16>(active_x * static_cast<float>(touchscreen_width)),
                .y = static_cast<u16>(active_y * static_cast<float>(touchscreen_height)),
            };
            touch_entry.diameter_x = Settings::values.touchscreen.diameter_x;
            touch_entry.diameter_y = Settings::values.touchscreen.diameter_y;
            touch_entry.rotation_angle = Settings::values.touchscreen.rotation_angle;
            touch_entry.delta_time = timestamp - active_fingers[id].last_touch;
            fingers[active_fingers[id].id].last_touch = timestamp;
            touch_entry.finger = active_fingers[id].id;
            touch_entry.attribute.raw = active_fingers[id].attribute.raw;
        } else {
            // Clear touch entry
            touch_entry.attribute.raw = 0;
            touch_entry.position = {};
            touch_entry.diameter_x = 0;
            touch_entry.diameter_y = 0;
            touch_entry.rotation_angle = 0;
            touch_entry.delta_time = 0;
            touch_entry.finger = 0;
        }
    }

    shared_memory.touch_screen_lifo.WriteNextEntry(next_state);
}

void TouchScreen::SetTouchscreenDimensions(u32 width, u32 height) {
    touchscreen_width = width;
    touchscreen_height = height;
}

} // namespace Service::HID
