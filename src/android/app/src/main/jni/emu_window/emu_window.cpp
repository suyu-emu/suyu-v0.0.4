#include <android/native_window_jni.h>

#include "common/logging/log.h"
#include "input_common/drivers/touch_screen.h"
#include "input_common/drivers/virtual_gamepad.h"
#include "input_common/main.h"
#include "jni/emu_window/emu_window.h"

void EmuWindow_Android::OnSurfaceChanged(ANativeWindow* surface) {
    render_window = surface;
}

bool EmuWindow_Android::OnTouchEvent(float x, float y, bool pressed) {
    if (pressed) {
        input_subsystem->GetTouchScreen()->TouchPressed(NormalizeX(x), NormalizeY(y), 0);
        return true;
    }

    input_subsystem->GetTouchScreen()->ReleaseAllTouch();
    return true;
}

void EmuWindow_Android::OnTouchMoved(float x, float y) {
    input_subsystem->GetTouchScreen()->TouchMoved(NormalizeX(x), NormalizeY(y), 0);
}

void EmuWindow_Android::OnGamepadEvent(int button_id, bool pressed) {
    input_subsystem->GetVirtualGamepad()->SetButtonState(0, button_id, pressed);
}

void EmuWindow_Android::OnGamepadMoveEvent(float x, float y) {
    input_subsystem->GetVirtualGamepad()->SetStickPosition(
        0, InputCommon::VirtualGamepad::VirtualStick::Left, x, y);
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

    host_window = surface_;
    window_info.type = Core::Frontend::WindowSystemType::Android;
    window_info.render_surface = reinterpret_cast<void*>(host_window);

    input_subsystem->Initialize();
}

EmuWindow_Android::~EmuWindow_Android() {
    input_subsystem->Shutdown();
}
