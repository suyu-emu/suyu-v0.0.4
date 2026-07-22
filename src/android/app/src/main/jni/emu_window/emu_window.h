// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <span>

#include "core/frontend/emu_window.h"
#include "core/frontend/graphics_context.h"
#include "input_common/main.h"

struct ANativeWindow;

class GraphicsContext_Android final : public Core::Frontend::GraphicsContext {
public:
    explicit GraphicsContext_Android(std::shared_ptr<Common::DynamicLibrary> driver_library)
        : m_driver_library{driver_library} {}

    ~GraphicsContext_Android() = default;

    std::shared_ptr<Common::DynamicLibrary> GetDriverLibrary() override {
        return m_driver_library;
    }

private:
    std::shared_ptr<Common::DynamicLibrary> m_driver_library;
};

class EmuWindow_Android final : public Core::Frontend::EmuWindow {

public:
    EmuWindow_Android(ANativeWindow* surface,
                      std::shared_ptr<Common::DynamicLibrary> driver_library);

    ~EmuWindow_Android() = default;

    void OnSurfaceChanged(ANativeWindow* surface);
    void OnFrameDisplayed() override;

    void OnTouchPressed(int id, float x, float y);
    void OnTouchMoved(int id, float x, float y);
    void OnTouchReleased(int id);

    std::unique_ptr<Core::Frontend::GraphicsContext> CreateSharedContext() const override {
        return {std::make_unique<GraphicsContext_Android>(m_driver_library)};
    }
    bool IsShown() const override {
        return true;
    };

private:
    using Clock = std::chrono::steady_clock;

    void UpdateFrameRateHint();
    void UpdateObservedFrameRate();
    [[nodiscard]] float GetFrameRateHint() const;
    [[nodiscard]] float GetFrameTimeVerifiedHint() const;
    [[nodiscard]] static float QuantizeFrameRateHint(float frame_rate);

    float m_window_width{};
    float m_window_height{};

    std::shared_ptr<Common::DynamicLibrary> m_driver_library;

    bool m_first_frame = false;
    float m_last_frame_rate_hint = -1.0f;
    float m_pending_frame_rate_hint = -1.0f;
    float m_smoothed_present_rate = 0.0f;
    Clock::time_point m_last_frame_display_time{};
    Clock::time_point m_pending_frame_rate_since{};
    std::uint32_t m_pending_frame_rate_hint_votes = 0;
};
