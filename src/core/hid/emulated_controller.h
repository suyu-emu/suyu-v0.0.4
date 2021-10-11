// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <mutex>
#include <unordered_map>

#include "common/input.h"
#include "common/param_package.h"
#include "common/point.h"
#include "common/quaternion.h"
#include "common/settings.h"
#include "common/vector_math.h"
#include "core/hid/hid_types.h"
#include "core/hid/motion_input.h"

namespace Core::HID {

struct ControllerMotionInfo {
    Input::MotionStatus raw_status;
    MotionInput emulated{};
};

using ButtonDevices =
    std::array<std::unique_ptr<Input::InputDevice>, Settings::NativeButton::NumButtons>;
using StickDevices =
    std::array<std::unique_ptr<Input::InputDevice>, Settings::NativeAnalog::NumAnalogs>;
using ControllerMotionDevices =
    std::array<std::unique_ptr<Input::InputDevice>, Settings::NativeMotion::NumMotions>;
using TriggerDevices =
    std::array<std::unique_ptr<Input::InputDevice>, Settings::NativeTrigger::NumTriggers>;
using BatteryDevices = std::array<std::unique_ptr<Input::InputDevice>, 2>;
using OutputDevices = std::array<std::unique_ptr<Input::OutputDevice>, 2>;

using ButtonParams = std::array<Common::ParamPackage, Settings::NativeButton::NumButtons>;
using StickParams = std::array<Common::ParamPackage, Settings::NativeAnalog::NumAnalogs>;
using ControllerMotionParams = std::array<Common::ParamPackage, Settings::NativeMotion::NumMotions>;
using TriggerParams = std::array<Common::ParamPackage, Settings::NativeTrigger::NumTriggers>;
using BatteryParams = std::array<Common::ParamPackage, 2>;
using OutputParams = std::array<Common::ParamPackage, 2>;

using ButtonValues = std::array<Input::ButtonStatus, Settings::NativeButton::NumButtons>;
using SticksValues = std::array<Input::StickStatus, Settings::NativeAnalog::NumAnalogs>;
using TriggerValues = std::array<Input::TriggerStatus, Settings::NativeTrigger::NumTriggers>;
using ControllerMotionValues = std::array<ControllerMotionInfo, Settings::NativeMotion::NumMotions>;
using ColorValues = std::array<Input::BodyColorStatus, 3>;
using BatteryValues = std::array<Input::BatteryStatus, 3>;
using VibrationValues = std::array<Input::VibrationStatus, 2>;

struct AnalogSticks {
    AnalogStickState left;
    AnalogStickState right;
};

struct ControllerColors {
    NpadControllerColor fullkey;
    NpadControllerColor left;
    NpadControllerColor right;
};

struct BatteryLevelState {
    NpadPowerInfo dual;
    NpadPowerInfo left;
    NpadPowerInfo right;
};

struct ControllerMotion {
    bool is_at_rest;
    Common::Vec3f accel{};
    Common::Vec3f gyro{};
    Common::Vec3f rotation{};
    std::array<Common::Vec3f, 3> orientation{};
};

using MotionState = std::array<ControllerMotion, 2>;

struct ControllerStatus {
    // Data from input_common
    ButtonValues button_values{};
    SticksValues stick_values{};
    ControllerMotionValues motion_values{};
    TriggerValues trigger_values{};
    ColorValues color_values{};
    BatteryValues battery_values{};
    VibrationValues vibration_values{};

    // Data for Nintendo devices
    NpadButtonState npad_button_state{};
    DebugPadButton debug_pad_button_state{};
    AnalogSticks analog_stick_state{};
    MotionState motion_state{};
    NpadGcTriggerState gc_trigger_state{};
    ControllerColors colors_state{};
    BatteryLevelState battery_state{};
};

enum class ControllerTriggerType {
    Button,
    Stick,
    Trigger,
    Motion,
    Color,
    Battery,
    Vibration,
    Connected,
    Disconnected,
    Type,
    All,
};

struct ControllerUpdateCallback {
    std::function<void(ControllerTriggerType)> on_change;
};

class EmulatedController {
public:
    /**
     * TODO: Write description
     *
     * @param npad_id_type
     */
    explicit EmulatedController(NpadIdType npad_id_type_);
    ~EmulatedController();

    YUZU_NON_COPYABLE(EmulatedController);
    YUZU_NON_MOVEABLE(EmulatedController);

    static NpadType MapSettingsTypeToNPad(Settings::ControllerType type);
    static Settings::ControllerType MapNPadToSettingsType(NpadType type);

    /// Gets the NpadIdType for this controller.
    NpadIdType GetNpadIdType() const;

    /// Sets the NpadType for this controller.
    void SetNpadType(NpadType npad_type_);

    /// Gets the NpadType for this controller.
    NpadType GetNpadType() const;

    /// Gets the NpadType for this controller.
    LedPattern GetLedPattern() const;

    void Connect();
    void Disconnect();

    bool IsConnected() const;
    bool IsVibrationEnabled() const;

    void ReloadFromSettings();
    void ReloadInput();
    void UnloadInput();

    void EnableConfiguration();
    void DisableConfiguration();
    bool IsConfiguring() const;
    void SaveCurrentConfig();
    void RestoreConfig();

    std::vector<Common::ParamPackage> GetMappedDevices() const;

    Common::ParamPackage GetButtonParam(std::size_t index) const;
    Common::ParamPackage GetStickParam(std::size_t index) const;
    Common::ParamPackage GetMotionParam(std::size_t index) const;

    void SetButtonParam(std::size_t index, Common::ParamPackage param);
    void SetStickParam(std::size_t index, Common::ParamPackage param);
    void SetMotionParam(std::size_t index, Common::ParamPackage param);

    ButtonValues GetButtonsValues() const;
    SticksValues GetSticksValues() const;
    TriggerValues GetTriggersValues() const;
    ControllerMotionValues GetMotionValues() const;
    ColorValues GetColorsValues() const;
    BatteryValues GetBatteryValues() const;

    NpadButtonState GetNpadButtons() const;
    DebugPadButton GetDebugPadButtons() const;
    AnalogSticks GetSticks() const;
    NpadGcTriggerState GetTriggers() const;
    MotionState GetMotions() const;
    ControllerColors GetColors() const;
    BatteryLevelState GetBattery() const;

    bool SetVibration(std::size_t device_index, VibrationValue vibration);
    bool TestVibration(std::size_t device_index);

    void SetLedPattern();

    int SetCallback(ControllerUpdateCallback update_callback);
    void DeleteCallback(int key);

private:
    /**
     * Sets the status of a button. Applies toggle properties to the output.
     *
     * @param A CallbackStatus and a button index number
     */
    void SetButton(Input::CallbackStatus callback, std::size_t index);
    void SetStick(Input::CallbackStatus callback, std::size_t index);
    void SetTrigger(Input::CallbackStatus callback, std::size_t index);
    void SetMotion(Input::CallbackStatus callback, std::size_t index);
    void SetBattery(Input::CallbackStatus callback, std::size_t index);

    /**
     * Triggers a callback that something has changed
     *
     * @param Input type of the trigger
     */
    void TriggerOnChange(ControllerTriggerType type);

    NpadIdType npad_id_type;
    NpadType npad_type{NpadType::None};
    bool is_connected{false};
    bool is_configuring{false};
    bool is_vibration_enabled{true};
    f32 motion_sensitivity{0.01f};

    ButtonParams button_params;
    StickParams stick_params;
    ControllerMotionParams motion_params;
    TriggerParams trigger_params;
    BatteryParams battery_params;
    OutputParams output_params;

    ButtonDevices button_devices;
    StickDevices stick_devices;
    ControllerMotionDevices motion_devices;
    TriggerDevices trigger_devices;
    BatteryDevices battery_devices;
    OutputDevices output_devices;

    mutable std::mutex mutex;
    std::unordered_map<int, ControllerUpdateCallback> callback_list;
    int last_callback_key = 0;
    ControllerStatus controller;
};

} // namespace Core::HID
