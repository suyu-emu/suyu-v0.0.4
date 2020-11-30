// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "common/math_util.h"
#include "common/param_package.h"
#include "input_common/mouse/mouse_input.h"

namespace MouseInput {

Mouse::Mouse() {
    update_thread = std::thread(&Mouse::UpdateThread, this);
}

Mouse::~Mouse() {
    update_thread_running = false;
    if (update_thread.joinable()) {
        update_thread.join();
    }
}

void Mouse::UpdateThread() {
    constexpr int update_time = 10;
    while (update_thread_running) {
        for (MouseInfo& info : mouse_info) {
            Common::Vec3f angular_direction = {-info.tilt_direction.y, 0.0f,
                                               -info.tilt_direction.x};

            info.motion.SetGyroscope(angular_direction * info.tilt_speed);
            info.motion.UpdateRotation(update_time * 1000);
            info.motion.UpdateOrientation(update_time * 1000);
            info.tilt_speed = 0;
            info.data.motion = info.motion.GetMotion();
        }
        if (configuring) {
            UpdateYuzuSettings();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(update_time));
    }
}

void Mouse::UpdateYuzuSettings() {
    MouseStatus pad_status{};
    if (buttons != 0) {
        pad_status.button = last_button;
        mouse_queue.Push(pad_status);
    }
}

void Mouse::PressButton(int x, int y, int button_) {
    if (button_ >= static_cast<int>(mouse_info.size())) {
        return;
    }

    int button = 1 << button_;
    buttons |= static_cast<u16>(button);
    last_button = static_cast<MouseButton>(button_);

    mouse_info[button_].mouse_origin = Common::MakeVec(x, y);
    mouse_info[button_].last_mouse_position = Common::MakeVec(x, y);
    mouse_info[button_].data.pressed = true;
}

void Mouse::MouseMove(int x, int y) {
    for (MouseInfo& info : mouse_info) {
        if (info.data.pressed) {
            auto mouse_move = Common::MakeVec(x, y) - info.mouse_origin;
            auto mouse_change = Common::MakeVec(x, y) - info.last_mouse_position;
            info.last_mouse_position = Common::MakeVec(x, y);
            info.data.axis = {mouse_move.x, -mouse_move.y};

            if (mouse_change.x == 0 && mouse_change.y == 0) {
                info.tilt_speed = 0;
            } else {
                info.tilt_direction = mouse_change.Cast<float>();
                info.tilt_speed = info.tilt_direction.Normalize() * info.sensitivity;
            }
        }
    }
}

void Mouse::ReleaseButton(int button_) {
    if (button_ >= static_cast<int>(mouse_info.size())) {
        return;
    }

    int button = 1 << button_;
    buttons &= static_cast<u16>(0xFF - button);

    mouse_info[button_].tilt_speed = 0;
    mouse_info[button_].data.pressed = false;
    mouse_info[button_].data.axis = {0, 0};
}

void Mouse::BeginConfiguration() {
    buttons = 0;
    last_button = MouseButton::Undefined;
    mouse_queue.Clear();
    configuring = true;
}

void Mouse::EndConfiguration() {
    buttons = 0;
    last_button = MouseButton::Undefined;
    mouse_queue.Clear();
    configuring = false;
}

Common::SPSCQueue<MouseStatus>& Mouse::GetMouseQueue() {
    return mouse_queue;
}

const Common::SPSCQueue<MouseStatus>& Mouse::GetMouseQueue() const {
    return mouse_queue;
}

MouseData& Mouse::GetMouseState(std::size_t button) {
    return mouse_info[button].data;
}

const MouseData& Mouse::GetMouseState(std::size_t button) const {
    return mouse_info[button].data;
}
} // namespace MouseInput
