// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "common/logging/log.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hid/hid_core.h"
#include "core/hle/kernel/k_shared_memory.h"
#include "core/hle/service/hid/resource_manager.h"
#include "core/hle/service/ipc_helpers.h"

#include "core/hle/service/hid/controllers/console_six_axis.h"
#include "core/hle/service/hid/controllers/debug_pad.h"
#include "core/hle/service/hid/controllers/gesture.h"
#include "core/hle/service/hid/controllers/keyboard.h"
#include "core/hle/service/hid/controllers/mouse.h"
#include "core/hle/service/hid/controllers/npad.h"
#include "core/hle/service/hid/controllers/palma.h"
#include "core/hle/service/hid/controllers/seven_six_axis.h"
#include "core/hle/service/hid/controllers/six_axis.h"
#include "core/hle/service/hid/controllers/stubbed.h"
#include "core/hle/service/hid/controllers/touchscreen.h"
#include "core/hle/service/hid/controllers/xpad.h"

namespace Service::HID {

// Updating period for each HID device.
// Period time is obtained by measuring the number of samples in a second on HW using a homebrew
// Correct npad_update_ns is 4ms this is overclocked to lower input lag
constexpr auto npad_update_ns = std::chrono::nanoseconds{1 * 1000 * 1000};    // (1ms, 1000Hz)
constexpr auto default_update_ns = std::chrono::nanoseconds{4 * 1000 * 1000}; // (4ms, 1000Hz)
constexpr auto mouse_keyboard_update_ns = std::chrono::nanoseconds{8 * 1000 * 1000}; // (8ms, 125Hz)
constexpr auto motion_update_ns = std::chrono::nanoseconds{5 * 1000 * 1000};         // (5ms, 200Hz)

ResourceManager::ResourceManager(Core::System& system_)
    : system{system_}, service_context{system_, "hid"} {}

ResourceManager::~ResourceManager() = default;

void ResourceManager::Initialize() {
    if (is_initialized) {
        return;
    }

    u8* shared_memory = system.Kernel().GetHidSharedMem().GetPointer();
    debug_pad = std::make_shared<DebugPad>(system.HIDCore(), shared_memory);
    mouse = std::make_shared<Mouse>(system.HIDCore(), shared_memory);
    debug_mouse = std::make_shared<DebugMouse>(system.HIDCore(), shared_memory);
    keyboard = std::make_shared<Keyboard>(system.HIDCore(), shared_memory);
    unique_pad = std::make_shared<UniquePad>(system.HIDCore(), shared_memory);
    npad = std::make_shared<NPad>(system.HIDCore(), shared_memory, service_context);
    gesture = std::make_shared<Gesture>(system.HIDCore(), shared_memory);
    touch_screen = std::make_shared<TouchScreen>(system.HIDCore(), shared_memory);
    xpad = std::make_shared<XPad>(system.HIDCore(), shared_memory);

    palma = std::make_shared<Palma>(system.HIDCore(), shared_memory, service_context);

    home_button = std::make_shared<HomeButton>(system.HIDCore(), shared_memory);
    sleep_button = std::make_shared<SleepButton>(system.HIDCore(), shared_memory);
    capture_button = std::make_shared<CaptureButton>(system.HIDCore(), shared_memory);

    six_axis = std::make_shared<SixAxis>(system.HIDCore(), npad);
    console_six_axis = std::make_shared<ConsoleSixAxis>(system.HIDCore(), shared_memory);
    seven_six_axis = std::make_shared<SevenSixAxis>(system);

    home_button->SetCommonHeaderOffset(0x4C00);
    sleep_button->SetCommonHeaderOffset(0x4E00);
    capture_button->SetCommonHeaderOffset(0x5000);
    unique_pad->SetCommonHeaderOffset(0x5A00);
    debug_mouse->SetCommonHeaderOffset(0x3DC00);

    // Homebrew doesn't try to activate some controllers, so we activate them by default
    npad->Activate();
    six_axis->Activate();
    touch_screen->Activate();

    system.HIDCore().ReloadInputDevices();
    is_initialized = true;
}
std::shared_ptr<CaptureButton> ResourceManager::GetCaptureButton() const {
    return capture_button;
}

std::shared_ptr<ConsoleSixAxis> ResourceManager::GetConsoleSixAxis() const {
    return console_six_axis;
}

std::shared_ptr<DebugMouse> ResourceManager::GetDebugMouse() const {
    return debug_mouse;
}

std::shared_ptr<DebugPad> ResourceManager::GetDebugPad() const {
    return debug_pad;
}

std::shared_ptr<Gesture> ResourceManager::GetGesture() const {
    return gesture;
}

std::shared_ptr<HomeButton> ResourceManager::GetHomeButton() const {
    return home_button;
}

std::shared_ptr<Keyboard> ResourceManager::GetKeyboard() const {
    return keyboard;
}

std::shared_ptr<Mouse> ResourceManager::GetMouse() const {
    return mouse;
}

std::shared_ptr<NPad> ResourceManager::GetNpad() const {
    return npad;
}

std::shared_ptr<Palma> ResourceManager::GetPalma() const {
    return palma;
}

std::shared_ptr<SevenSixAxis> ResourceManager::GetSevenSixAxis() const {
    return seven_six_axis;
}

std::shared_ptr<SixAxis> ResourceManager::GetSixAxis() const {
    return six_axis;
}

std::shared_ptr<SleepButton> ResourceManager::GetSleepButton() const {
    return sleep_button;
}

std::shared_ptr<TouchScreen> ResourceManager::GetTouchScreen() const {
    return touch_screen;
}

std::shared_ptr<UniquePad> ResourceManager::GetUniquePad() const {
    return unique_pad;
}

void ResourceManager::UpdateControllers(std::uintptr_t user_data,
                                        std::chrono::nanoseconds ns_late) {
    auto& core_timing = system.CoreTiming();
    debug_pad->OnUpdate(core_timing);
    unique_pad->OnUpdate(core_timing);
    gesture->OnUpdate(core_timing);
    touch_screen->OnUpdate(core_timing);
    palma->OnUpdate(core_timing);
    home_button->OnUpdate(core_timing);
    sleep_button->OnUpdate(core_timing);
    capture_button->OnUpdate(core_timing);
    xpad->OnUpdate(core_timing);
}

void ResourceManager::UpdateNpad(std::uintptr_t user_data, std::chrono::nanoseconds ns_late) {
    auto& core_timing = system.CoreTiming();
    npad->OnUpdate(core_timing);
}

void ResourceManager::UpdateMouseKeyboard(std::uintptr_t user_data,
                                          std::chrono::nanoseconds ns_late) {
    auto& core_timing = system.CoreTiming();
    mouse->OnUpdate(core_timing);
    debug_mouse->OnUpdate(core_timing);
    keyboard->OnUpdate(core_timing);
}

void ResourceManager::UpdateMotion(std::uintptr_t user_data, std::chrono::nanoseconds ns_late) {
    auto& core_timing = system.CoreTiming();
    six_axis->OnUpdate(core_timing);
    seven_six_axis->OnUpdate(core_timing);
    console_six_axis->OnUpdate(core_timing);
}

IAppletResource::IAppletResource(Core::System& system_, std::shared_ptr<ResourceManager> resource)
    : ServiceFramework{system_, "IAppletResource"} {
    static const FunctionInfo functions[] = {
        {0, &IAppletResource::GetSharedMemoryHandle, "GetSharedMemoryHandle"},
    };
    RegisterHandlers(functions);

    resource->Initialize();

    // Register update callbacks
    npad_update_event = Core::Timing::CreateEvent(
        "HID::UpdatePadCallback",
        [this, resource](std::uintptr_t user_data, s64 time, std::chrono::nanoseconds ns_late)
            -> std::optional<std::chrono::nanoseconds> {
            const auto guard = LockService();
            resource->UpdateNpad(user_data, ns_late);
            return std::nullopt;
        });
    default_update_event = Core::Timing::CreateEvent(
        "HID::UpdateDefaultCallback",
        [this, resource](std::uintptr_t user_data, s64 time, std::chrono::nanoseconds ns_late)
            -> std::optional<std::chrono::nanoseconds> {
            const auto guard = LockService();
            resource->UpdateControllers(user_data, ns_late);
            return std::nullopt;
        });
    mouse_keyboard_update_event = Core::Timing::CreateEvent(
        "HID::UpdateMouseKeyboardCallback",
        [this, resource](std::uintptr_t user_data, s64 time, std::chrono::nanoseconds ns_late)
            -> std::optional<std::chrono::nanoseconds> {
            const auto guard = LockService();
            resource->UpdateMouseKeyboard(user_data, ns_late);
            return std::nullopt;
        });
    motion_update_event = Core::Timing::CreateEvent(
        "HID::UpdateMotionCallback",
        [this, resource](std::uintptr_t user_data, s64 time, std::chrono::nanoseconds ns_late)
            -> std::optional<std::chrono::nanoseconds> {
            const auto guard = LockService();
            resource->UpdateMotion(user_data, ns_late);
            return std::nullopt;
        });

    system.CoreTiming().ScheduleLoopingEvent(npad_update_ns, npad_update_ns, npad_update_event);
    system.CoreTiming().ScheduleLoopingEvent(default_update_ns, default_update_ns,
                                             default_update_event);
    system.CoreTiming().ScheduleLoopingEvent(mouse_keyboard_update_ns, mouse_keyboard_update_ns,
                                             mouse_keyboard_update_event);
    system.CoreTiming().ScheduleLoopingEvent(motion_update_ns, motion_update_ns,
                                             motion_update_event);
}

IAppletResource::~IAppletResource() {
    system.CoreTiming().UnscheduleEvent(npad_update_event, 0);
    system.CoreTiming().UnscheduleEvent(default_update_event, 0);
    system.CoreTiming().UnscheduleEvent(mouse_keyboard_update_event, 0);
    system.CoreTiming().UnscheduleEvent(motion_update_event, 0);
}

void IAppletResource::GetSharedMemoryHandle(HLERequestContext& ctx) {
    LOG_DEBUG(Service_HID, "called");

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(&system.Kernel().GetHidSharedMem());
}

} // namespace Service::HID
