// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

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
    EmuWindow_Android(InputCommon::InputSubsystem* input_subsystem, ANativeWindow* surface,
                      std::shared_ptr<Common::DynamicLibrary> driver_library);

    ~EmuWindow_Android();

    void OnSurfaceChanged(ANativeWindow* surface);
    void OnTouchPressed(int id, float x, float y);
    void OnTouchMoved(int id, float x, float y);
    void OnTouchReleased(int id);
    void OnGamepadButtonEvent(int player_index, int button_id, bool pressed);
    void OnGamepadJoystickEvent(int player_index, int stick_id, float x, float y);
    void OnGamepadMotionEvent(int player_index, u64 delta_timestamp, float gyro_x, float gyro_y,
                              float gyro_z, float accel_x, float accel_y, float accel_z);
    void OnReadNfcTag(std::span<u8> data);
    void OnRemoveNfcTag();
    void OnFrameDisplayed() override;

    std::unique_ptr<Core::Frontend::GraphicsContext> CreateSharedContext() const override {
        return {std::make_unique<GraphicsContext_Android>(m_driver_library)};
    }
    bool IsShown() const override {
        return true;
    };

private:
    InputCommon::InputSubsystem* m_input_subsystem{};

    float m_window_width{};
    float m_window_height{};

    std::shared_ptr<Common::DynamicLibrary> m_driver_library;

    bool m_first_frame = false;
};
