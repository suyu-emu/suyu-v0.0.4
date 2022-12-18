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
    bool OnTouchEvent(float x, float y, bool pressed);
    void OnTouchMoved(float x, float y);
    void OnGamepadEvent(int button, bool pressed);
    void OnGamepadMoveEvent(float x, float y);
    void OnFrameDisplayed() override {}

    std::unique_ptr<Core::Frontend::GraphicsContext> CreateSharedContext() const override {
        return {std::make_unique<SharedContext_Android>()};
    }
    bool IsShown() const override {
        return true;
    };

private:
    float NormalizeX(float x) const {
        return std::clamp(x / window_width, 0.f, 1.f);
    }

    float NormalizeY(float y) const {
        return std::clamp(y / window_height, 0.f, 1.f);
    }

    InputCommon::InputSubsystem* input_subsystem{};

    ANativeWindow* render_window{};
    ANativeWindow* host_window{};

    float window_width{};
    float window_height{};
};
