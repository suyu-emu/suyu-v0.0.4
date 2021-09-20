// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include <thread>
#include "common/param_package.h"
#include "common/settings.h"
#include "input_common/main.h"
#ifdef HAVE_SDL2
#include "input_common/sdl/sdl.h"
#endif

namespace InputCommon {

struct InputSubsystem::Impl {
    void Initialize() {
    }

    void Shutdown() {
    }

    [[nodiscard]] std::vector<Common::ParamPackage> GetInputDevices() const {
        std::vector<Common::ParamPackage> devices = {
            Common::ParamPackage{{"display", "Any"}, {"class", "any"}},
            Common::ParamPackage{{"display", "Keyboard/Mouse"}, {"class", "keyboard"}},
        };
        return {};
    }

    [[nodiscard]] AnalogMapping GetAnalogMappingForDevice(
        const Common::ParamPackage& params) const {
        if (!params.Has("class") || params.Get("class", "") == "any") {
            return {};
        }
        return {};
    }

    [[nodiscard]] ButtonMapping GetButtonMappingForDevice(
        const Common::ParamPackage& params) const {
        if (!params.Has("class") || params.Get("class", "") == "any") {
            return {};
        }
        return {};
    }

    [[nodiscard]] MotionMapping GetMotionMappingForDevice(
        const Common::ParamPackage& params) const {
        if (!params.Has("class") || params.Get("class", "") == "any") {
            return {};
        }
        return {};
    }

};

InputSubsystem::InputSubsystem() : impl{std::make_unique<Impl>()} {}

InputSubsystem::~InputSubsystem() = default;

void InputSubsystem::Initialize() {
    impl->Initialize();
}

void InputSubsystem::Shutdown() {
    impl->Shutdown();
}

std::vector<Common::ParamPackage> InputSubsystem::GetInputDevices() const {
    return impl->GetInputDevices();
}

AnalogMapping InputSubsystem::GetAnalogMappingForDevice(const Common::ParamPackage& device) const {
    return impl->GetAnalogMappingForDevice(device);
}

ButtonMapping InputSubsystem::GetButtonMappingForDevice(const Common::ParamPackage& device) const {
    return impl->GetButtonMappingForDevice(device);
}

MotionMapping InputSubsystem::GetMotionMappingForDevice(const Common::ParamPackage& device) const {
    return impl->GetMotionMappingForDevice(device);
}

void InputSubsystem::ReloadInputDevices() {
}

std::vector<std::unique_ptr<Polling::DevicePoller>> InputSubsystem::GetPollers([
    [maybe_unused]] Polling::DeviceType type) const {
    return {};
}

std::string GenerateKeyboardParam(int key_code) {
    Common::ParamPackage param;
    param.Set("engine", "keyboard");
    param.Set("code", key_code);
    param.Set("toggle", false);
    return param.Serialize();
}

std::string GenerateAnalogParamFromKeys(int key_up, int key_down, int key_left, int key_right,
                                        int key_modifier, float modifier_scale) {
    Common::ParamPackage circle_pad_param{
        {"engine", "analog_from_button"},
        {"up", GenerateKeyboardParam(key_up)},
        {"down", GenerateKeyboardParam(key_down)},
        {"left", GenerateKeyboardParam(key_left)},
        {"right", GenerateKeyboardParam(key_right)},
        {"modifier", GenerateKeyboardParam(key_modifier)},
        {"modifier_scale", std::to_string(modifier_scale)},
    };
    return circle_pad_param.Serialize();
}
} // namespace InputCommon
