// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/core.h"
#include "core/hle/service/am/am_types.h"
#include "core/hle/service/am/button_poller.h"
#include "core/hle/service/am/window_system.h"
#include "hid_core/frontend/emulated_controller.h"
#include "hid_core/hid_core.h"
#include "hid_core/hid_types.h"

namespace Service::AM {

namespace {

ButtonPressDuration ClassifyPressDuration(std::chrono::steady_clock::time_point start) {
    using namespace std::chrono_literals;

    const auto dur = std::chrono::steady_clock::now() - start;

    // TODO: determine actual thresholds
    // TODO: these are likely different for each button
    if (dur < 400ms) {
        return ButtonPressDuration::ShortPressing;
    } else if (dur < 800ms) {
        return ButtonPressDuration::MiddlePressing;
    } else {
        return ButtonPressDuration::LongPressing;
    }
}

} // namespace

ButtonPoller::ButtonPoller(Core::System& system, WindowSystem& window_system)
    : m_window_system(window_system) {
    // TODO: am reads this from the home button state in hid, which is controller-agnostic.
    Core::HID::ControllerUpdateCallback engine_callback{
        .on_change =
            [this](Core::HID::ControllerTriggerType type) {
                if (type == Core::HID::ControllerTriggerType::Button) {
                    this->OnButtonStateChanged();
                }
            },
        .is_npad_service = true,
    };

    m_handheld = system.HIDCore().GetEmulatedController(Core::HID::NpadIdType::Handheld);
    m_handheld_key = m_handheld->SetCallback(engine_callback);
    m_player1 = system.HIDCore().GetEmulatedController(Core::HID::NpadIdType::Player1);
    m_player1_key = m_player1->SetCallback(engine_callback);

    m_thread = std::thread([this] { this->ThreadLoop(); });
}

ButtonPoller::~ButtonPoller() {
    m_handheld->DeleteCallback(m_handheld_key);
    m_player1->DeleteCallback(m_player1_key);
    m_stop = true;
    m_cv.notify_all();
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void ButtonPoller::OnButtonStateChanged() {
    std::lock_guard lk{m_mutex};
    const bool home_button =
        m_handheld->GetHomeButtons().home.Value() || m_player1->GetHomeButtons().home.Value();
    const bool capture_button = m_handheld->GetCaptureButtons().capture.Value() ||
                                m_player1->GetCaptureButtons().capture.Value();

    // Buttons pressed which were not previously pressed
    if (home_button && !m_home_button_press_start) {
        m_home_button_press_start = std::chrono::steady_clock::now();
        m_home_button_long_sent = false;
    }
    if (capture_button && !m_capture_button_press_start) {
        m_capture_button_press_start = std::chrono::steady_clock::now();
        m_capture_button_long_sent = false;
    }
    // if (power_button && !m_power_button_press_start) {
    //     m_power_button_press_start = std::chrono::steady_clock::now();
    // }

    // While buttons are held, check whether they have crossed the long-press
    // threshold and, if so, send the long-press action immediately (only once).
    if (home_button && m_home_button_press_start && !m_home_button_long_sent) {
        const auto duration = ClassifyPressDuration(*m_home_button_press_start);
        if (duration != ButtonPressDuration::ShortPressing) {
            m_window_system.OnSystemButtonPress(SystemButtonType::HomeButtonLongPressing);
            m_home_button_long_sent = true;
        }
     }

    if (capture_button && m_capture_button_press_start && !m_capture_button_long_sent) {
         const auto duration = ClassifyPressDuration(*m_capture_button_press_start);
         if (duration != ButtonPressDuration::ShortPressing) {
             m_window_system.OnSystemButtonPress(SystemButtonType::CaptureButtonLongPressing);
             m_capture_button_long_sent = true;
         }
     }

    // Buttons released which were previously held
    if (!home_button && m_home_button_press_start) {
        if(!m_home_button_long_sent) {
            const auto duration = ClassifyPressDuration(*m_home_button_press_start);
            m_window_system.OnSystemButtonPress(
                duration == ButtonPressDuration::ShortPressing ? SystemButtonType::HomeButtonShortPressing
                                                               : SystemButtonType::HomeButtonLongPressing);
        }
        m_home_button_press_start = std::nullopt;
        m_home_button_long_sent = false;
    }
    if (!capture_button && m_capture_button_press_start) {
        if (!m_capture_button_long_sent) {
            const auto duration = ClassifyPressDuration(*m_capture_button_press_start);
            m_window_system.OnSystemButtonPress(
                duration == ButtonPressDuration::ShortPressing ? SystemButtonType::CaptureButtonShortPressing
                                                               : SystemButtonType::CaptureButtonLongPressing);
        }
        m_capture_button_press_start = std::nullopt;
        m_capture_button_long_sent = false;
    }
    // if (!power_button && m_power_button_press_start) {
    //     // TODO
    //     m_power_button_press_start = std::nullopt;
    // }
}

void ButtonPoller::ThreadLoop() {
    using namespace std::chrono_literals;
    std::unique_lock lk{m_mutex};
    while (!m_stop) {
        m_cv.wait_for(lk, 50ms);
        if (m_stop) break;
        lk.unlock();
        OnButtonStateChanged();
        lk.lock();
    }
}

} // namespace Service::AM
