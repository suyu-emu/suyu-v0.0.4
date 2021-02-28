// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include "common/common_types.h"
#include "core/core_timing.h"
#include "core/frontend/emu_window.h"
#include "core/hle/service/hid/controllers/gesture.h"
#include "core/settings.h"

namespace Service::HID {
constexpr std::size_t SHARED_MEMORY_OFFSET = 0x3BA00;
constexpr f32 angle_threshold = 0.08f;
constexpr f32 pinch_threshold = 100.0f;

Controller_Gesture::Controller_Gesture(Core::System& system) : ControllerBase(system) {}
Controller_Gesture::~Controller_Gesture() = default;

void Controller_Gesture::OnInit() {
    for (std::size_t id = 0; id < MAX_FINGERS; ++id) {
        mouse_finger_id[id] = MAX_FINGERS;
        keyboard_finger_id[id] = MAX_FINGERS;
        udp_finger_id[id] = MAX_FINGERS;
    }
}

void Controller_Gesture::OnRelease() {}

void Controller_Gesture::OnUpdate(const Core::Timing::CoreTiming& core_timing, u8* data,
                                  std::size_t size) {
    shared_memory.header.timestamp = core_timing.GetCPUTicks();
    shared_memory.header.total_entry_count = 17;

    if (!IsControllerActivated()) {
        shared_memory.header.entry_count = 0;
        shared_memory.header.last_entry_index = 0;
        return;
    }
    shared_memory.header.entry_count = 16;

    const auto& last_entry = shared_memory.gesture_states[shared_memory.header.last_entry_index];
    shared_memory.header.last_entry_index = (shared_memory.header.last_entry_index + 1) % 17;
    auto& cur_entry = shared_memory.gesture_states[shared_memory.header.last_entry_index];

    cur_entry.sampling_number = last_entry.sampling_number + 1;
    cur_entry.sampling_number2 = cur_entry.sampling_number;

    // TODO(german77): Implement all gesture types

    const Input::TouchStatus& mouse_status = touch_mouse_device->GetStatus();
    const Input::TouchStatus& udp_status = touch_udp_device->GetStatus();
    for (std::size_t id = 0; id < mouse_status.size(); ++id) {
        mouse_finger_id[id] = UpdateTouchInputEvent(mouse_status[id], mouse_finger_id[id]);
        udp_finger_id[id] = UpdateTouchInputEvent(udp_status[id], udp_finger_id[id]);
    }

    if (Settings::values.use_touch_from_button) {
        const Input::TouchStatus& keyboard_status = touch_btn_device->GetStatus();
        for (std::size_t id = 0; id < mouse_status.size(); ++id) {
            keyboard_finger_id[id] =
                UpdateTouchInputEvent(keyboard_status[id], keyboard_finger_id[id]);
        }
    }

    TouchType type = TouchType::Idle;
    Attribute attributes{};
    GestureProperties gesture = GetGestureProperties();
    if (last_gesture.active_points != gesture.active_points) {
        ++last_gesture.detection_count;
    }
    if (gesture.active_points > 0) {
        if (last_gesture.active_points == 0) {
            attributes.is_new_touch.Assign(true);
            last_gesture.average_distance = gesture.average_distance;
            last_gesture.angle = gesture.angle;
        }

        type = TouchType::Touch;
        if (gesture.mid_point.x != last_entry.x || gesture.mid_point.y != last_entry.y) {
            type = TouchType::Pan;
        }
        if (std::abs(gesture.average_distance - last_gesture.average_distance) > pinch_threshold) {
            type = TouchType::Pinch;
        }
        if (std::abs(gesture.angle - last_gesture.angle) > angle_threshold) {
            type = TouchType::Rotate;
        }

        cur_entry.delta_x = gesture.mid_point.x - last_entry.x;
        cur_entry.delta_y = gesture.mid_point.y - last_entry.y;
        // TODO: Find how velocities are calculated
        cur_entry.vel_x = static_cast<float>(cur_entry.delta_x) * 150.1f;
        cur_entry.vel_y = static_cast<float>(cur_entry.delta_y) * 150.1f;

        // Slowdown the rate of change for less flapping
        last_gesture.average_distance =
            (last_gesture.average_distance * 0.9f) + (gesture.average_distance * 0.1f);
        last_gesture.angle = (last_gesture.angle * 0.9f) + (gesture.angle * 0.1f);

    } else {
        cur_entry.delta_x = 0;
        cur_entry.delta_y = 0;
        cur_entry.vel_x = 0;
        cur_entry.vel_y = 0;
    }
    last_gesture.active_points = gesture.active_points;
    cur_entry.detection_count = last_gesture.detection_count;
    cur_entry.type = type;
    cur_entry.attributes = attributes;
    cur_entry.x = gesture.mid_point.x;
    cur_entry.y = gesture.mid_point.y;
    cur_entry.point_count = static_cast<s32>(gesture.active_points);
    for (size_t id = 0; id < MAX_POINTS; id++) {
        cur_entry.points[id].x = gesture.points[id].x;
        cur_entry.points[id].y = gesture.points[id].y;
    }
    cur_entry.rotation_angle = 0;
    cur_entry.scale = 0;

    std::memcpy(data + SHARED_MEMORY_OFFSET, &shared_memory, sizeof(SharedMemory));
}

void Controller_Gesture::OnLoadInputDevices() {
    touch_mouse_device = Input::CreateDevice<Input::TouchDevice>("engine:emu_window");
    touch_udp_device = Input::CreateDevice<Input::TouchDevice>("engine:cemuhookudp");
    touch_btn_device = Input::CreateDevice<Input::TouchDevice>("engine:touch_from_button");
}

std::optional<std::size_t> Controller_Gesture::GetUnusedFingerID() const {
    std::size_t first_free_id = 0;
    while (first_free_id < MAX_POINTS) {
        if (!fingers[first_free_id].pressed) {
            return first_free_id;
        } else {
            first_free_id++;
        }
    }
    return std::nullopt;
}

std::size_t Controller_Gesture::UpdateTouchInputEvent(
    const std::tuple<float, float, bool>& touch_input, std::size_t finger_id) {
    const auto& [x, y, pressed] = touch_input;
    if (pressed) {
        if (finger_id == MAX_POINTS) {
            const auto first_free_id = GetUnusedFingerID();
            if (!first_free_id) {
                // Invalid finger id do nothing
                return MAX_POINTS;
            }
            finger_id = first_free_id.value();
            fingers[finger_id].pressed = true;
        }
        fingers[finger_id].x = x;
        fingers[finger_id].y = y;
        return finger_id;
    }

    if (finger_id != MAX_POINTS) {
        fingers[finger_id].pressed = false;
    }

    return MAX_POINTS;
}

Controller_Gesture::GestureProperties Controller_Gesture::GetGestureProperties() {
    GestureProperties gesture;
    std::array<Finger, MAX_POINTS> active_fingers;
    const auto end_iter = std::copy_if(fingers.begin(), fingers.end(), active_fingers.begin(),
                                       [](const auto& finger) { return finger.pressed; });
    gesture.active_points =
        static_cast<std::size_t>(std::distance(active_fingers.begin(), end_iter));

    for (size_t id = 0; id < gesture.active_points; ++id) {
        gesture.points[id].x =
            static_cast<int>(active_fingers[id].x * Layout::ScreenUndocked::Width);
        gesture.points[id].y =
            static_cast<int>(active_fingers[id].y * Layout::ScreenUndocked::Height);
        gesture.mid_point.x += static_cast<int>(gesture.points[id].x / gesture.active_points);
        gesture.mid_point.y += static_cast<int>(gesture.points[id].y / gesture.active_points);
    }

    for (size_t id = 0; id < gesture.active_points; ++id) {
        const double distance =
            std::pow(static_cast<float>(gesture.mid_point.x - gesture.points[id].x), 2) +
            std::pow(static_cast<float>(gesture.mid_point.y - gesture.points[id].y), 2);
        gesture.average_distance +=
            static_cast<float>(distance) / static_cast<float>(gesture.active_points);
    }

    gesture.angle = std::atan2(static_cast<float>(gesture.mid_point.y - gesture.points[0].y),
                               static_cast<float>(gesture.mid_point.x - gesture.points[0].x));
    return gesture;
}

} // namespace Service::HID
