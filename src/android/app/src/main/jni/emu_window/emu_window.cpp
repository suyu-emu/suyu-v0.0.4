// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <android/native_window_jni.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <dlfcn.h>

#include "common/android/id_cache.h"
#include "common/logging.h"
#include "common/settings.h"
#include "input_common/drivers/android.h"
#include "input_common/drivers/touch_screen.h"
#include "input_common/drivers/virtual_amiibo.h"
#include "input_common/drivers/virtual_gamepad.h"
#include "input_common/main.h"
#include "jni/emu_window/emu_window.h"
#include "jni/native.h"

void EmuWindow_Android::OnSurfaceChanged(ANativeWindow* surface) {
    if (!surface) {
        LOG_INFO(Frontend, "EmuWindow_Android::OnSurfaceChanged received null surface");
        m_window_width = 0;
        m_window_height = 0;
        window_info.render_surface = nullptr;
        m_last_frame_rate_hint = -1.0f;
        m_pending_frame_rate_hint = -1.0f;
        m_pending_frame_rate_hint_votes = 0;
        m_smoothed_present_rate = 0.0f;
        m_last_frame_display_time = {};
        m_pending_frame_rate_since = {};
        return;
    }

    m_window_width = ANativeWindow_getWidth(surface);
    m_window_height = ANativeWindow_getHeight(surface);

    // Ensures that we emulate with the correct aspect ratio.
    UpdateCurrentFramebufferLayout(m_window_width, m_window_height);

    window_info.render_surface = reinterpret_cast<void*>(surface);
    UpdateFrameRateHint();
}

void EmuWindow_Android::OnTouchPressed(int id, float x, float y) {
    const auto [touch_x, touch_y] = MapToTouchScreen(x, y);
    EmulationSession::GetInstance().GetInputSubsystem().GetTouchScreen()->TouchPressed(touch_x,
                                                                                       touch_y, id);
}

void EmuWindow_Android::OnTouchMoved(int id, float x, float y) {
    const auto [touch_x, touch_y] = MapToTouchScreen(x, y);
    EmulationSession::GetInstance().GetInputSubsystem().GetTouchScreen()->TouchMoved(touch_x,
                                                                                     touch_y, id);
}

void EmuWindow_Android::OnTouchReleased(int id) {
    EmulationSession::GetInstance().GetInputSubsystem().GetTouchScreen()->TouchReleased(id);
}

void EmuWindow_Android::OnFrameDisplayed() {
    UpdateObservedFrameRate();
    UpdateFrameRateHint();

    if (!m_first_frame) {
        Common::Android::RunJNIOnFiber<void>(
            [&](JNIEnv* env) { EmulationSession::GetInstance().OnEmulationStarted(); });
        m_first_frame = true;
    }
}

void EmuWindow_Android::UpdateObservedFrameRate() {
    const auto now = Clock::now();
    if (m_last_frame_display_time.time_since_epoch().count() != 0) {
        const auto frame_time = std::chrono::duration<float>(now - m_last_frame_display_time);
        const float seconds = frame_time.count();
        if (seconds > 0.0f) {
            const float instantaneous_rate = 1.0f / seconds;
            if (std::isfinite(instantaneous_rate) && instantaneous_rate >= 1.0f &&
                instantaneous_rate <= 240.0f) {
                constexpr float SmoothingFactor = 0.15f;
                if (m_smoothed_present_rate <= 0.0f) {
                    m_smoothed_present_rate = instantaneous_rate;
                } else {
                    m_smoothed_present_rate +=
                        (instantaneous_rate - m_smoothed_present_rate) * SmoothingFactor;
                }
            }
        }
    }
    m_last_frame_display_time = now;
}

float EmuWindow_Android::QuantizeFrameRateHint(float frame_rate) {
    if (!std::isfinite(frame_rate) || frame_rate <= 0.0f) {
        return 0.0f;
    }

    frame_rate = std::clamp(frame_rate, 1.0f, 240.0f);

    constexpr float Step = 0.5f;
    return std::round(frame_rate / Step) * Step;
}

float EmuWindow_Android::GetFrameTimeVerifiedHint() const {
    if (!EmulationSession::GetInstance().IsRunning()) {
        return 0.0f;
    }

    const double frame_time_scale =
        EmulationSession::GetInstance().System().GetPerfStats().GetLastFrameTimeScale();
    if (!std::isfinite(frame_time_scale) || frame_time_scale <= 0.0) {
        return 0.0f;
    }

    const float verified_rate =
        std::clamp(60.0f / static_cast<float>(frame_time_scale), 0.0f, 240.0f);
    return QuantizeFrameRateHint(verified_rate);
}

float EmuWindow_Android::GetFrameRateHint() const {
    const float observed_rate = std::clamp(m_smoothed_present_rate, 0.0f, 240.0f);
    const float frame_time_verified_hint = GetFrameTimeVerifiedHint();

    if (m_last_frame_rate_hint > 0.0f && observed_rate > 0.0f) {
        const float tolerance = std::max(m_last_frame_rate_hint * 0.12f, 4.0f);
        if (std::fabs(observed_rate - m_last_frame_rate_hint) <= tolerance) {
            return m_last_frame_rate_hint;
        }
    }

    const float observed_hint = QuantizeFrameRateHint(observed_rate);
    if (observed_hint > 0.0f) {
        if (frame_time_verified_hint > 0.0f) {
            const float tolerance = std::max(observed_hint * 0.20f, 3.0f);
            if (std::fabs(observed_hint - frame_time_verified_hint) <= tolerance) {
                return QuantizeFrameRateHint((observed_hint + frame_time_verified_hint) * 0.5f);
            }
        }
        return observed_hint;
    }

    if (frame_time_verified_hint > 0.0f) {
        return frame_time_verified_hint;
    }

    constexpr float NominalFrameRate = 60.0f;
    if (!Settings::values.use_speed_limit.GetValue()) {
        return NominalFrameRate;
    }

    const u16 speed_limit = Settings::SpeedLimit();
    if (speed_limit == 0) {
        return 0.0f;
    }

    const float speed_limited_rate =
        NominalFrameRate * (static_cast<float>(std::min<u16>(speed_limit, 100)) / 100.0f);
    return QuantizeFrameRateHint(speed_limited_rate);
}

void EmuWindow_Android::UpdateFrameRateHint() {
    auto* const surface = reinterpret_cast<ANativeWindow*>(window_info.render_surface);
    if (!surface) {
        return;
    }

    const auto now = Clock::now();
    const float frame_rate_hint = GetFrameRateHint();
    if (std::fabs(frame_rate_hint - m_last_frame_rate_hint) < 0.01f) {
        m_pending_frame_rate_hint = frame_rate_hint;
        m_pending_frame_rate_hint_votes = 0;
        m_pending_frame_rate_since = {};
        return;
    }

    if (frame_rate_hint == 0.0f) {
        m_pending_frame_rate_hint = frame_rate_hint;
        m_pending_frame_rate_hint_votes = 0;
        m_pending_frame_rate_since = now;
    } else if (m_last_frame_rate_hint >= 0.0f) {
        if (std::fabs(frame_rate_hint - m_pending_frame_rate_hint) >= 0.01f) {
            m_pending_frame_rate_hint = frame_rate_hint;
            m_pending_frame_rate_hint_votes = 1;
            m_pending_frame_rate_since = now;
            return;
        }

        ++m_pending_frame_rate_hint_votes;
        if (m_pending_frame_rate_since.time_since_epoch().count() == 0) {
            m_pending_frame_rate_since = now;
        }

        const auto stable_for = now - m_pending_frame_rate_since;
        const float reference_rate = std::max(frame_rate_hint, 1.0f);
        const auto stable_duration = std::chrono::duration_cast<Clock::duration>(
            std::chrono::duration<float>(std::clamp(3.0f / reference_rate, 0.15f, 0.40f)));
        constexpr std::uint32_t MinStableVotes = 3;

        if (m_pending_frame_rate_hint_votes < MinStableVotes || stable_for < stable_duration) {
            return;
        }
    } else {
        m_pending_frame_rate_since = now;
    }

    using SetFrameRateWithChangeStrategyFn =
        int32_t (*)(ANativeWindow*, float, int8_t, int8_t);
    using SetFrameRateFn = int32_t (*)(ANativeWindow*, float, int8_t);
    static const auto set_frame_rate_with_change_strategy =
        reinterpret_cast<SetFrameRateWithChangeStrategyFn>(
            dlsym(RTLD_DEFAULT, "ANativeWindow_setFrameRateWithChangeStrategy"));
    static const auto set_frame_rate = reinterpret_cast<SetFrameRateFn>(
        dlsym(RTLD_DEFAULT, "ANativeWindow_setFrameRate"));

    constexpr int8_t FrameRateCompatibilityDefault = 0;
    constexpr int8_t ChangeFrameRateOnlyIfSeamless = 0;

    int32_t result = -1;
    if (set_frame_rate_with_change_strategy) {
        result = set_frame_rate_with_change_strategy(surface, frame_rate_hint,
                                                     FrameRateCompatibilityDefault,
                                                     ChangeFrameRateOnlyIfSeamless);
    } else if (set_frame_rate) {
        result = set_frame_rate(surface, frame_rate_hint, FrameRateCompatibilityDefault);
    } else {
        return;
    }

    if (result != 0) {
        LOG_DEBUG(Frontend, "Failed to update Android surface frame rate hint: {}", result);
        return;
    }

    m_last_frame_rate_hint = frame_rate_hint;
    m_pending_frame_rate_hint = frame_rate_hint;
    m_pending_frame_rate_hint_votes = 0;
    m_pending_frame_rate_since = {};
}

EmuWindow_Android::EmuWindow_Android(ANativeWindow* surface,
                                     std::shared_ptr<Common::DynamicLibrary> driver_library)
    : m_driver_library{driver_library} {
    LOG_INFO(Frontend, "initializing");

    if (!surface) {
        LOG_CRITICAL(Frontend, "surface is nullptr");
        return;
    }

    OnSurfaceChanged(surface);
    window_info.type = Core::Frontend::WindowSystemType::Android;
}
