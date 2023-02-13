#pragma once

#include "core/frontend/emu_window.h"
#include "input_common/main.h"

struct ANativeWindow;

class SharedContext_Android : public Core::Frontend::GraphicsContext {
public:
    SharedContext_Android() = default;
    ~SharedContext_Android() = default;
    void MakeCurrent() override {}
    void DoneCurrent() override {}
};

class EmuWindow_Android : public Core::Frontend::EmuWindow {
public:
    EmuWindow_Android(InputCommon::InputSubsystem* input_subsystem_, ANativeWindow* surface_);
    ~EmuWindow_Android();

    void OnSurfaceChanged(ANativeWindow* surface);
    void OnTouchPressed(int id, float x, float y);
    void OnTouchMoved(int id, float x, float y);
    void OnTouchReleased(int id);
    void OnGamepadButtonEvent(int player_index, int button_id, bool pressed);
    void OnGamepadJoystickEvent(int player_index, int stick_id, float x, float y);
    void OnGamepadMotionEvent(int player_index, u64 delta_timestamp, float gyro_x, float gyro_y,
                              float gyro_z, float accel_x, float accel_y, float accel_z);
    void OnFrameDisplayed() override {}

    std::unique_ptr<Core::Frontend::GraphicsContext> CreateSharedContext() const override {
        return {std::make_unique<SharedContext_Android>()};
    }
    bool IsShown() const override {
        return true;
    };

private:
    InputCommon::InputSubsystem* input_subsystem{};

    ANativeWindow* render_window{};
    ANativeWindow* host_window{};

    float window_width{};
    float window_height{};
};
