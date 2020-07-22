// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "input_common/gcadapter/gc_poller.h"
#include "input_common/settings.h"

namespace Common {
class ParamPackage;
}

namespace InputCommon {

/// Initializes and registers all built-in input device factories.
void Init();

/// Deregisters all built-in input device factories and shuts them down.
void Shutdown();

class Keyboard;

/// Gets the keyboard button device factory.
Keyboard* GetKeyboard();

class MotionEmu;

/// Gets the motion emulation factory.
MotionEmu* GetMotionEmu();

GCButtonFactory* GetGCButtons();

GCAnalogFactory* GetGCAnalogs();

/// Generates a serialized param package for creating a keyboard button device
std::string GenerateKeyboardParam(int key_code);

/// Generates a serialized param package for creating an analog device taking input from keyboard
std::string GenerateAnalogParamFromKeys(int key_up, int key_down, int key_left, int key_right,
                                        int key_modifier, float modifier_scale);

/**
 * Return a list of available input devices that this Factory can create a new device with.
 * Each returned Parampackage should have a `display` field used for display, a class field for
 * backends to determine if this backend is meant to service the request and any other information
 * needed to identify this in the backend later.
 */
std::vector<Common::ParamPackage> GetInputDevices();

/**
 * Given a ParamPackage for a Device returned from `GetInputDevices`, attempt to get the default
 * mapping for the device. This is currently only implemented for the sdl backend devices.
 */
using ButtonMapping = std::unordered_map<Settings::NativeButton::Values, Common::ParamPackage>;
using AnalogMapping = std::unordered_map<Settings::NativeAnalog::Values, Common::ParamPackage>;

ButtonMapping GetButtonMappingForDevice(const Common::ParamPackage&);
AnalogMapping GetAnalogMappingForDevice(const Common::ParamPackage&);

namespace Polling {

enum class DeviceType { Button, AnalogPreferred };

/**
 * A class that can be used to get inputs from an input device like controllers without having to
 * poll the device's status yourself
 */
class DevicePoller {
public:
    virtual ~DevicePoller() = default;
    /// Setup and start polling for inputs, should be called before GetNextInput
    /// If a device_id is provided, events should be filtered to only include events from this
    /// device id
    virtual void Start(std::string device_id = "") = 0;
    /// Stop polling
    virtual void Stop() = 0;
    /**
     * Every call to this function returns the next input recorded since calling Start
     * @return A ParamPackage of the recorded input, which can be used to create an InputDevice.
     *         If there has been no input, the package is empty
     */
    virtual Common::ParamPackage GetNextInput() = 0;
};

// Get all DevicePoller from all backends for a specific device type
std::vector<std::unique_ptr<DevicePoller>> GetPollers(DeviceType type);
} // namespace Polling
} // namespace InputCommon
