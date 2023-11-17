// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/hle/service/kernel_helpers.h"
#include "core/hle/service/service.h"

namespace Core::Timing {
struct EventType;
}

namespace Service::HID {
class Controller_Stubbed;
class ConsoleSixAxis;
class DebugPad;
class Gesture;
class Keyboard;
class Mouse;
class NPad;
class Palma;
class SevenSixAxis;
class SixAxis;
class TouchScreen;
class XPad;

using CaptureButton = Controller_Stubbed;
using DebugMouse = Controller_Stubbed;
using HomeButton = Controller_Stubbed;
using SleepButton = Controller_Stubbed;
using UniquePad = Controller_Stubbed;

class ResourceManager {

public:
    explicit ResourceManager(Core::System& system_);
    ~ResourceManager();

    void Initialize();

    std::shared_ptr<CaptureButton> GetCaptureButton() const;
    std::shared_ptr<ConsoleSixAxis> GetConsoleSixAxis() const;
    std::shared_ptr<DebugMouse> GetDebugMouse() const;
    std::shared_ptr<DebugPad> GetDebugPad() const;
    std::shared_ptr<Gesture> GetGesture() const;
    std::shared_ptr<HomeButton> GetHomeButton() const;
    std::shared_ptr<Keyboard> GetKeyboard() const;
    std::shared_ptr<Mouse> GetMouse() const;
    std::shared_ptr<NPad> GetNpad() const;
    std::shared_ptr<Palma> GetPalma() const;
    std::shared_ptr<SevenSixAxis> GetSevenSixAxis() const;
    std::shared_ptr<SixAxis> GetSixAxis() const;
    std::shared_ptr<SleepButton> GetSleepButton() const;
    std::shared_ptr<TouchScreen> GetTouchScreen() const;
    std::shared_ptr<UniquePad> GetUniquePad() const;

    void UpdateControllers(std::uintptr_t user_data, std::chrono::nanoseconds ns_late);
    void UpdateNpad(std::uintptr_t user_data, std::chrono::nanoseconds ns_late);
    void UpdateMouseKeyboard(std::uintptr_t user_data, std::chrono::nanoseconds ns_late);
    void UpdateMotion(std::uintptr_t user_data, std::chrono::nanoseconds ns_late);

private:
    bool is_initialized{false};

    std::shared_ptr<CaptureButton> capture_button = nullptr;
    std::shared_ptr<ConsoleSixAxis> console_six_axis = nullptr;
    std::shared_ptr<DebugMouse> debug_mouse = nullptr;
    std::shared_ptr<DebugPad> debug_pad = nullptr;
    std::shared_ptr<Gesture> gesture = nullptr;
    std::shared_ptr<HomeButton> home_button = nullptr;
    std::shared_ptr<Keyboard> keyboard = nullptr;
    std::shared_ptr<Mouse> mouse = nullptr;
    std::shared_ptr<NPad> npad = nullptr;
    std::shared_ptr<Palma> palma = nullptr;
    std::shared_ptr<SevenSixAxis> seven_six_axis = nullptr;
    std::shared_ptr<SixAxis> six_axis = nullptr;
    std::shared_ptr<SleepButton> sleep_button = nullptr;
    std::shared_ptr<TouchScreen> touch_screen = nullptr;
    std::shared_ptr<UniquePad> unique_pad = nullptr;
    std::shared_ptr<XPad> xpad = nullptr;

    // TODO: Create these resources
    // std::shared_ptr<AudioControl> audio_control = nullptr;
    // std::shared_ptr<ButtonConfig> button_config = nullptr;
    // std::shared_ptr<Config> config = nullptr;
    // std::shared_ptr<Connection> connection = nullptr;
    // std::shared_ptr<CustomConfig> custom_config = nullptr;
    // std::shared_ptr<Digitizer> digitizer = nullptr;
    // std::shared_ptr<Hdls> hdls = nullptr;
    // std::shared_ptr<PlayReport> play_report = nullptr;
    // std::shared_ptr<Rail> rail = nullptr;

    Core::System& system;
    KernelHelpers::ServiceContext service_context;
};

class IAppletResource final : public ServiceFramework<IAppletResource> {
public:
    explicit IAppletResource(Core::System& system_, std::shared_ptr<ResourceManager> resource);
    ~IAppletResource() override;

private:
    void GetSharedMemoryHandle(HLERequestContext& ctx);

    std::shared_ptr<Core::Timing::EventType> npad_update_event;
    std::shared_ptr<Core::Timing::EventType> default_update_event;
    std::shared_ptr<Core::Timing::EventType> mouse_keyboard_update_event;
    std::shared_ptr<Core::Timing::EventType> motion_update_event;
};

} // namespace Service::HID
