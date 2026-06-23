// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <thread>
#include "common/thread.h"
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

ButtonPoller::ButtonPoller(Core::System& system, WindowSystem& window_system) {
    // TODO: am reads this from the home button state in hid, which is controller-agnostic.
    Core::HID::ControllerUpdateCallback engine_callback{
        .on_change = [this, &window_system](Core::HID::ControllerTriggerType type) {
            if (type == Core::HID::ControllerTriggerType::Button) {
                std::unique_lock lk{m_mutex};
                OnButtonStateChanged(window_system);
            }
        },
        .is_npad_service = true,
    };

    m_handheld = system.HIDCore().GetEmulatedController(Core::HID::NpadIdType::Handheld);
    m_handheld_key = m_handheld->SetCallback(engine_callback);
    m_player1 = system.HIDCore().GetEmulatedController(Core::HID::NpadIdType::Player1);
    m_player1_key = m_player1->SetCallback(engine_callback);

    m_thread = std::jthread([this, &window_system](std::stop_token stop_token) {
        Common::SetCurrentThreadName("ButtonPoller");
        while (!stop_token.stop_requested()) {
            using namespace std::chrono_literals;
            std::unique_lock lk{m_mutex};
            m_cv.wait_for(lk, 50ms);
            if (stop_token.stop_requested())
                break;
            OnButtonStateChanged(window_system);
            std::this_thread::sleep_for(5ms);
        }
    });
}

ButtonPoller::~ButtonPoller() {
    m_handheld->DeleteCallback(m_handheld_key);
    m_player1->DeleteCallback(m_player1_key);
    m_cv.notify_all();
    if (m_thread.joinable()) {
        m_thread.request_stop();
        m_thread.join();
    }
}

void ButtonPoller::OnButtonStateChanged(WindowSystem& window_system) {
    auto const home_button = m_handheld->GetHomeButtons().home.Value()
        || m_player1->GetHomeButtons().home.Value();
    auto const capture_button = m_handheld->GetCaptureButtons().capture.Value()
        || m_player1->GetCaptureButtons().capture.Value();

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
            window_system.OnSystemButtonPress(SystemButtonType::HomeButtonLongPressing);
            m_home_button_long_sent = true;
        }
     }

    if (capture_button && m_capture_button_press_start && !m_capture_button_long_sent) {
         const auto duration = ClassifyPressDuration(*m_capture_button_press_start);
         if (duration != ButtonPressDuration::ShortPressing) {
             window_system.OnSystemButtonPress(SystemButtonType::CaptureButtonLongPressing);
             m_capture_button_long_sent = true;
         }
     }

    // Buttons released which were previously held
    if (!home_button && m_home_button_press_start) {
        if(!m_home_button_long_sent) {
            const auto duration = ClassifyPressDuration(*m_home_button_press_start);
            window_system.OnSystemButtonPress(duration == ButtonPressDuration::ShortPressing
                ? SystemButtonType::HomeButtonShortPressing : SystemButtonType::HomeButtonLongPressing);
        }
        m_home_button_press_start = std::nullopt;
        m_home_button_long_sent = false;
    }
    if (!capture_button && m_capture_button_press_start) {
        if (!m_capture_button_long_sent) {
            const auto duration = ClassifyPressDuration(*m_capture_button_press_start);
            window_system.OnSystemButtonPress(duration == ButtonPressDuration::ShortPressing
                ? SystemButtonType::CaptureButtonShortPressing : SystemButtonType::CaptureButtonLongPressing);
        }
        m_capture_button_press_start = std::nullopt;
        m_capture_button_long_sent = false;
    }
    // if (!power_button && m_power_button_press_start) {
    //     // TODO
    //     m_power_button_press_start = std::nullopt;
    // }
}

} // namespace Service::AM
