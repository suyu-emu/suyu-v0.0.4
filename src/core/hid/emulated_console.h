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

struct ConsoleMotionInfo {
    Input::MotionStatus raw_status;
    MotionInput emulated{};
};

using ConsoleMotionDevices = std::unique_ptr<Input::InputDevice>;
using TouchDevices = std::array<std::unique_ptr<Input::InputDevice>, 16>;

using ConsoleMotionParams = Common::ParamPackage;
using TouchParams = std::array<Common::ParamPackage, 16>;

using ConsoleMotionValues = ConsoleMotionInfo;
using TouchValues = std::array<Input::TouchStatus, 16>;

struct TouchFinger {
    u64_le last_touch{};
    Common::Point<float> position{};
    u32_le id{};
    bool pressed{};
    Core::HID::TouchAttribute attribute{};
};

struct ConsoleMotion {
    bool is_at_rest{};
    Common::Vec3f accel{};
    Common::Vec3f gyro{};
    Common::Vec3f rotation{};
    std::array<Common::Vec3f, 3> orientation{};
    Common::Quaternion<f32> quaternion{};
};

using TouchFingerState = std::array<TouchFinger, 16>;

struct ConsoleStatus {
    // Data from input_common
    ConsoleMotionValues motion_values{};
    TouchValues touch_values{};

    // Data for Nintendo devices;
    ConsoleMotion motion_state{};
    TouchFingerState touch_state{};
};

enum class ConsoleTriggerType {
    Motion,
    Touch,
    All,
};

struct ConsoleUpdateCallback {
    std::function<void(ConsoleTriggerType)> on_change;
};

class EmulatedConsole {
public:
    /**
     * TODO: Write description
     *
     * @param npad_id_type
     */
    explicit EmulatedConsole();
    ~EmulatedConsole();

    YUZU_NON_COPYABLE(EmulatedConsole);
    YUZU_NON_MOVEABLE(EmulatedConsole);

    void ReloadFromSettings();
    void ReloadInput();
    void UnloadInput();

    void EnableConfiguration();
    void DisableConfiguration();
    bool IsConfiguring() const;
    void SaveCurrentConfig();
    void RestoreConfig();

    Common::ParamPackage GetMotionParam() const;

    void SetMotionParam(Common::ParamPackage param);

    ConsoleMotionValues GetMotionValues() const;
    TouchValues GetTouchValues() const;

    ConsoleMotion GetMotion() const;
    TouchFingerState GetTouch() const;

    int SetCallback(ConsoleUpdateCallback update_callback);
    void DeleteCallback(int key);

private:
    /**
     * Sets the status of a button. Applies toggle properties to the output.
     *
     * @param A CallbackStatus and a button index number
     */
    void SetMotion(Input::CallbackStatus callback);
    void SetTouch(Input::CallbackStatus callback, std::size_t index);

    /**
     * Triggers a callback that something has changed
     *
     * @param Input type of the trigger
     */
    void TriggerOnChange(ConsoleTriggerType type);

    bool is_configuring{false};
    f32 motion_sensitivity{0.01f};

    ConsoleMotionParams motion_params;
    TouchParams touch_params;

    ConsoleMotionDevices motion_devices;
    TouchDevices touch_devices;

    mutable std::mutex mutex;
    std::unordered_map<int, ConsoleUpdateCallback> callback_list;
    int last_callback_key = 0;
    ConsoleStatus console;
};

} // namespace Core::HID
