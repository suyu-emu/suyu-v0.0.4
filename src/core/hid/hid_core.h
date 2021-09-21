// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include "core/hid/emulated_console.h"
#include "core/hid/emulated_controller.h"
#include "core/hid/emulated_devices.h"

namespace Core::HID {

class HIDCore {
public:
    explicit HIDCore();
    ~HIDCore();

    YUZU_NON_COPYABLE(HIDCore);
    YUZU_NON_MOVEABLE(HIDCore);

    EmulatedController* GetEmulatedController(NpadIdType npad_id_type);
    const EmulatedController* GetEmulatedController(NpadIdType npad_id_type) const;

    EmulatedController* GetEmulatedControllerByIndex(std::size_t index);
    const EmulatedController* GetEmulatedControllerByIndex(std::size_t index) const;

    EmulatedConsole* GetEmulatedConsole();
    const EmulatedConsole* GetEmulatedConsole() const;

    EmulatedDevices* GetEmulatedDevices();
    const EmulatedDevices* GetEmulatedDevices() const;

    void SetSupportedStyleTag(NpadStyleTag style_tag);
    NpadStyleTag GetSupportedStyleTag() const;

    // Reloads all input devices from settings
    void ReloadInputDevices();

    // Removes all callbacks from input common
    void UnloadInputDevices();

private:
    std::unique_ptr<EmulatedController> player_1;
    std::unique_ptr<EmulatedController> player_2;
    std::unique_ptr<EmulatedController> player_3;
    std::unique_ptr<EmulatedController> player_4;
    std::unique_ptr<EmulatedController> player_5;
    std::unique_ptr<EmulatedController> player_6;
    std::unique_ptr<EmulatedController> player_7;
    std::unique_ptr<EmulatedController> player_8;
    std::unique_ptr<EmulatedController> other;
    std::unique_ptr<EmulatedController> handheld;
    std::unique_ptr<EmulatedConsole> console;
    std::unique_ptr<EmulatedDevices> devices;
    NpadStyleTag supported_style_tag;
};

} // namespace Core::HID
