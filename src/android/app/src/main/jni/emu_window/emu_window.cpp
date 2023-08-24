// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <android/native_window_jni.h>

#include "common/logging/log.h"
#include "input_common/drivers/touch_screen.h"
#include "input_common/drivers/virtual_amiibo.h"
#include "input_common/drivers/virtual_gamepad.h"
#include "input_common/main.h"
#include "jni/emu_window/emu_window.h"

void EmuWindow_Android::OnSurfaceChanged(ANativeWindow* surface) {
    m_window_width = ANativeWindow_getWidth(surface);
    m_window_height = ANativeWindow_getHeight(surface);

    // Ensures that we emulate with the correct aspect ratio.
    UpdateCurrentFramebufferLayout(m_window_width, m_window_height);

    window_info.render_surface = reinterpret_cast<void*>(surface);
}

void EmuWindow_Android::OnTouchPressed(int id, float x, float y) {
    const auto [touch_x, touch_y] = MapToTouchScreen(x, y);
    m_input_subsystem->GetTouchScreen()->TouchPressed(touch_x, touch_y, id);
}

void EmuWindow_Android::OnTouchMoved(int id, float x, float y) {
    const auto [touch_x, touch_y] = MapToTouchScreen(x, y);
    m_input_subsystem->GetTouchScreen()->TouchMoved(touch_x, touch_y, id);
}

void EmuWindow_Android::OnTouchReleased(int id) {
    m_input_subsystem->GetTouchScreen()->TouchReleased(id);
}

void EmuWindow_Android::OnGamepadButtonEvent(int player_index, int button_id, bool pressed) {
    m_input_subsystem->GetVirtualGamepad()->SetButtonState(player_index, button_id, pressed);
}

void EmuWindow_Android::OnGamepadJoystickEvent(int player_index, int stick_id, float x, float y) {
    m_input_subsystem->GetVirtualGamepad()->SetStickPosition(player_index, stick_id, x, y);
}

void EmuWindow_Android::OnGamepadMotionEvent(int player_index, u64 delta_timestamp, float gyro_x,
                                             float gyro_y, float gyro_z, float accel_x,
                                             float accel_y, float accel_z) {
    m_input_subsystem->GetVirtualGamepad()->SetMotionState(
        player_index, delta_timestamp, gyro_x, gyro_y, gyro_z, accel_x, accel_y, accel_z);
}

void EmuWindow_Android::OnReadNfcTag(std::span<u8> data) {
    m_input_subsystem->GetVirtualAmiibo()->LoadAmiibo(data);
}

void EmuWindow_Android::OnRemoveNfcTag() {
    m_input_subsystem->GetVirtualAmiibo()->CloseAmiibo();
}

EmuWindow_Android::EmuWindow_Android(InputCommon::InputSubsystem* input_subsystem,
                                     ANativeWindow* surface,
                                     std::shared_ptr<Common::DynamicLibrary> driver_library)
    : m_input_subsystem{input_subsystem}, m_driver_library{driver_library} {
    LOG_INFO(Frontend, "initializing");

    if (!surface) {
        LOG_CRITICAL(Frontend, "surface is nullptr");
        return;
    }

    OnSurfaceChanged(surface);
    window_info.type = Core::Frontend::WindowSystemType::Android;

    m_input_subsystem->Initialize();
}

EmuWindow_Android::~EmuWindow_Android() {
    m_input_subsystem->Shutdown();
}
