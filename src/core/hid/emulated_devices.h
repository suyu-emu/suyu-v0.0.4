// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <mutex>
#include <unordered_map>

#include "common/input.h"
#include "common/param_package.h"
#include "common/settings.h"
#include "core/hid/hid_types.h"
#include "core/hid/motion_input.h"

namespace Core::HID {

using KeyboardDevices =
    std::array<std::unique_ptr<Input::InputDevice>, Settings::NativeKeyboard::NumKeyboardKeys>;
using KeyboardModifierDevices =
    std::array<std::unique_ptr<Input::InputDevice>, Settings::NativeKeyboard::NumKeyboardMods>;
using MouseButtonDevices =
    std::array<std::unique_ptr<Input::InputDevice>, Settings::NativeMouseButton::NumMouseButtons>;

using MouseButtonParams =
    std::array<Common::ParamPackage, Settings::NativeMouseButton::NumMouseButtons>;

using KeyboardValues = std::array<Input::ButtonStatus, Settings::NativeKeyboard::NumKeyboardKeys>;
using KeyboardModifierValues =
    std::array<Input::ButtonStatus, Settings::NativeKeyboard::NumKeyboardMods>;
using MouseButtonValues =
    std::array<Input::ButtonStatus, Settings::NativeMouseButton::NumMouseButtons>;

struct MousePosition {
    s32 x;
    s32 y;
    s32 delta_wheel_x;
    s32 delta_wheel_y;
};

struct DeviceStatus {
    // Data from input_common
    KeyboardValues keyboard_values{};
    KeyboardModifierValues keyboard_moddifier_values{};
    MouseButtonValues mouse_button_values{};

    // Data for Nintendo devices
    KeyboardKey keyboard_state{};
    KeyboardModifier keyboard_moddifier_state{};
    MouseButton mouse_button_state{};
    MousePosition mouse_position_state{};
};

enum class DeviceTriggerType {
    Keyboard,
    KeyboardModdifier,
    Mouse,
};

struct InterfaceUpdateCallback {
    std::function<void(DeviceTriggerType)> on_change;
};

class EmulatedDevices {
public:
    /**
     * TODO: Write description
     *
     * @param npad_id_type
     */
    explicit EmulatedDevices();
    ~EmulatedDevices();

    YUZU_NON_COPYABLE(EmulatedDevices);
    YUZU_NON_MOVEABLE(EmulatedDevices);

    void ReloadFromSettings();
    void ReloadInput();
    void UnloadInput();

    void EnableConfiguration();
    void DisableConfiguration();
    bool IsConfiguring() const;
    void SaveCurrentConfig();
    void RestoreConfig();

    std::vector<Common::ParamPackage> GetMappedDevices() const;

    Common::ParamPackage GetMouseButtonParam(std::size_t index) const;

    void SetButtonParam(std::size_t index, Common::ParamPackage param);

    KeyboardValues GetKeyboardValues() const;
    KeyboardModifierValues GetKeyboardModdifierValues() const;
    MouseButtonValues GetMouseButtonsValues() const;

    KeyboardKey GetKeyboard() const;
    KeyboardModifier GetKeyboardModifier() const;
    MouseButton GetMouseButtons() const;
    MousePosition GetMousePosition() const;

    int SetCallback(InterfaceUpdateCallback update_callback);
    void DeleteCallback(int key);

private:
    /**
     * Sets the status of a button. Applies toggle properties to the output.
     *
     * @param A CallbackStatus and a button index number
     */
    void SetKeyboardButton(Input::CallbackStatus callback, std::size_t index);
    void SetKeyboardModifier(Input::CallbackStatus callback, std::size_t index);
    void SetMouseButton(Input::CallbackStatus callback, std::size_t index);

    /**
     * Triggers a callback that something has changed
     *
     * @param Input type of the trigger
     */
    void TriggerOnChange(DeviceTriggerType type);

    bool is_configuring{false};

    MouseButtonParams mouse_button_params;

    KeyboardDevices keyboard_devices;
    KeyboardModifierDevices keyboard_modifier_devices;
    MouseButtonDevices mouse_button_devices;

    mutable std::mutex mutex;
    std::unordered_map<int, InterfaceUpdateCallback> callback_list;
    int last_callback_key = 0;
    DeviceStatus device_status;
};

} // namespace Core::HID
