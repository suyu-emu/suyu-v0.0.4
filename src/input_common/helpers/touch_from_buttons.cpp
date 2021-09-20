// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include "common/settings.h"
#include "core/frontend/framebuffer_layout.h"
#include "input_common/helpers/touch_from_buttons.h"

namespace InputCommon {

class TouchFromButtonDevice final : public Input::InputDevice {
public:
    using Button = std::unique_ptr<Input::InputDevice>;
    TouchFromButtonDevice(Button button_, u32 touch_id_, float x_, float y_)
        : button(std::move(button_)), touch_id(touch_id_), x(x_), y(y_) {
        Input::InputCallback button_up_callback{
            [this](Input::CallbackStatus callback_) { UpdateButtonStatus(callback_); }};
        button->SetCallback(button_up_callback);
    }

    Input::TouchStatus GetStatus(bool pressed) const {
        const Input::ButtonStatus button_status{
            .value = pressed,
        };
        Input::TouchStatus status{
            .pressed = button_status,
            .x = {},
            .y = {},
            .id = touch_id,
        };
        status.x.properties = properties;
        status.y.properties = properties;

        if (!pressed) {
            return status;
        }

        status.x.raw_value = x;
        status.y.raw_value = y;
        return status;
    }

    void UpdateButtonStatus(Input::CallbackStatus button_callback) {
        const Input::CallbackStatus status{
            .type = Input::InputType::Touch,
            .touch_status = GetStatus(button_callback.button_status.value),
        };
        TriggerOnChange(status);
    }

private:
    Button button;
    const u32 touch_id;
    const float x;
    const float y;
    const Input::AnalogProperties properties{0.0f, 1.0f, 0.5f, 0.0f, false};
};

std::unique_ptr<Input::InputDevice> TouchFromButton::Create(const Common::ParamPackage& params) {
    const std::string null_engine = Common::ParamPackage{{"engine", "null"}}.Serialize();
    auto button =
        Input::CreateDeviceFromString<Input::InputDevice>(params.Get("button", null_engine));
    const auto touch_id = params.Get("touch_id", 0);
    const float x = params.Get("x", 0.0f) / 1280.0f;
    const float y = params.Get("y", 0.0f) / 720.0f;
    return std::make_unique<TouchFromButtonDevice>(std::move(button), touch_id, x, y);
}

} // namespace InputCommon
