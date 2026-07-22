// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>
#include "hid_core/frontend/emulated_controller.h"

namespace Core {
namespace HID {
class EmulatedController;
}

class System;
} // namespace Core

namespace Service::AM {

class WindowSystem;

class ButtonPoller {
public:
    explicit ButtonPoller(Core::System& system, WindowSystem& window_system);
    ~ButtonPoller();
    void OnButtonStateChanged(WindowSystem& window_system);

private:
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::jthread m_thread;
    std::optional<std::chrono::steady_clock::time_point> m_home_button_press_start{};
    std::optional<std::chrono::steady_clock::time_point> m_capture_button_press_start{};
    std::optional<std::chrono::steady_clock::time_point> m_power_button_press_start{};

    Core::HID::EmulatedController* m_handheld{};
    Core::HID::EmulatedController* m_player1{};
    int32_t m_handheld_key{};
    int32_t m_player1_key{};
    bool m_home_button_long_sent : 1 = false;
    bool m_capture_button_long_sent : 1 = false;
    bool m_power_button_long_sent : 1 = false;
};

} // namespace Service::AM
