#include <android/native_window_jni.h>

#include "common/logging/log.h"
#include "input_common/drivers/touch_screen.h"
#include "input_common/drivers/virtual_gamepad.h"
#include "input_common/main.h"
#include "jni/emu_window/emu_window.h"

void EmuWindow_Android::OnSurfaceChanged(ANativeWindow* surface) {
    render_window = surface;
}

void EmuWindow_Android::OnTouchPressed(int id, float x, float y) {
    const auto [touch_x,touch_y]=MapToTouchScreen(x,y);
    input_subsystem->GetTouchScreen()->TouchPressed(touch_x, touch_y, id);
}

void EmuWindow_Android::OnTouchMoved(int id, float x, float y) {
    const auto [touch_x,touch_y]=MapToTouchScreen(x,y);
    input_subsystem->GetTouchScreen()->TouchMoved(touch_x, touch_y, id);
}

void EmuWindow_Android::OnTouchReleased(int id) {
    input_subsystem->GetTouchScreen()->TouchReleased(id);
}

void EmuWindow_Android::OnGamepadButtonEvent(int player_index, int button_id, bool pressed) {
    input_subsystem->GetVirtualGamepad()->SetButtonState(player_index, button_id, pressed);
}

void EmuWindow_Android::OnGamepadJoystickEvent(int player_index, int stick_id, float x, float y) {
    input_subsystem->GetVirtualGamepad()->SetStickPosition(
            player_index, stick_id, x, y);
}

void EmuWindow_Android::OnGamepadMotionEvent(int player_index, u64 delta_timestamp, float gyro_x, float gyro_y,
                                             float gyro_z, float accel_x, float accel_y,
                                             float accel_z) {
    // TODO:
    //  input_subsystem->GetVirtualGamepad()->SetMotionState(player_index, delta_timestamp, gyro_x, gyro_y,
    //                                                     gyro_z, accel_x, accel_y, accel_z);
}

EmuWindow_Android::EmuWindow_Android(InputCommon::InputSubsystem* input_subsystem_,
                                     ANativeWindow* surface_)
        : input_subsystem{input_subsystem_} {
    LOG_INFO(Frontend, "initializing");

    if (!surface_) {
        LOG_CRITICAL(Frontend, "surface is nullptr");
        return;
    }

    window_width = ANativeWindow_getWidth(surface_);
    window_height = ANativeWindow_getHeight(surface_);

    // Ensures that we emulate with the correct aspect ratio.
    UpdateCurrentFramebufferLayout(window_width, window_height);

    host_window = surface_;
    window_info.type = Core::Frontend::WindowSystemType::Android;
    window_info.render_surface = reinterpret_cast<void*>(host_window);

    input_subsystem->Initialize();
}

EmuWindow_Android::~EmuWindow_Android() {
    input_subsystem->Shutdown();
}
