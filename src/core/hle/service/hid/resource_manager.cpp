// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "common/logging/log.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hid/hid_core.h"
#include "core/hle/kernel/k_shared_memory.h"
#include "core/hle/service/hid/resource_manager.h"
#include "core/hle/service/ipc_helpers.h"

#include "core/hle/service/hid/controllers/console_sixaxis.h"
#include "core/hle/service/hid/controllers/controller_base.h"
#include "core/hle/service/hid/controllers/debug_pad.h"
#include "core/hle/service/hid/controllers/gesture.h"
#include "core/hle/service/hid/controllers/keyboard.h"
#include "core/hle/service/hid/controllers/mouse.h"
#include "core/hle/service/hid/controllers/npad.h"
#include "core/hle/service/hid/controllers/palma.h"
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
    MakeController<Controller_DebugPad>(HidController::DebugPad, shared_memory);
    MakeController<Controller_Touchscreen>(HidController::Touchscreen, shared_memory);
    MakeController<Controller_Mouse>(HidController::Mouse, shared_memory);
    MakeController<Controller_Keyboard>(HidController::Keyboard, shared_memory);
    MakeController<Controller_XPad>(HidController::XPad, shared_memory);
    MakeController<Controller_Stubbed>(HidController::HomeButton, shared_memory);
    MakeController<Controller_Stubbed>(HidController::SleepButton, shared_memory);
    MakeController<Controller_Stubbed>(HidController::CaptureButton, shared_memory);
    MakeController<Controller_Stubbed>(HidController::InputDetector, shared_memory);
    MakeController<Controller_Stubbed>(HidController::UniquePad, shared_memory);
    MakeControllerWithServiceContext<Controller_NPad>(HidController::NPad, shared_memory);
    MakeController<Controller_Gesture>(HidController::Gesture, shared_memory);
    MakeController<Controller_ConsoleSixAxis>(HidController::ConsoleSixAxisSensor, shared_memory);
    MakeController<Controller_Stubbed>(HidController::DebugMouse, shared_memory);
    MakeControllerWithServiceContext<Controller_Palma>(HidController::Palma, shared_memory);

    // Homebrew doesn't try to activate some controllers, so we activate them by default
    GetController<Controller_NPad>(HidController::NPad).ActivateController();
    GetController<Controller_Touchscreen>(HidController::Touchscreen).ActivateController();

    GetController<Controller_Stubbed>(HidController::HomeButton).SetCommonHeaderOffset(0x4C00);
    GetController<Controller_Stubbed>(HidController::SleepButton).SetCommonHeaderOffset(0x4E00);
    GetController<Controller_Stubbed>(HidController::CaptureButton).SetCommonHeaderOffset(0x5000);
    GetController<Controller_Stubbed>(HidController::InputDetector).SetCommonHeaderOffset(0x5200);
    GetController<Controller_Stubbed>(HidController::UniquePad).SetCommonHeaderOffset(0x5A00);
    GetController<Controller_Stubbed>(HidController::DebugMouse).SetCommonHeaderOffset(0x3DC00);

    system.HIDCore().ReloadInputDevices();
    is_initialized = true;
}

void ResourceManager::ActivateController(HidController controller) {
    controllers[static_cast<size_t>(controller)]->ActivateController();
}

void ResourceManager::DeactivateController(HidController controller) {
    controllers[static_cast<size_t>(controller)]->DeactivateController();
}

void ResourceManager::UpdateControllers(std::uintptr_t user_data,
                                        std::chrono::nanoseconds ns_late) {
    auto& core_timing = system.CoreTiming();

    for (const auto& controller : controllers) {
        // Keyboard has it's own update event
        if (controller == controllers[static_cast<size_t>(HidController::Keyboard)]) {
            continue;
        }
        // Mouse has it's own update event
        if (controller == controllers[static_cast<size_t>(HidController::Mouse)]) {
            continue;
        }
        // Npad has it's own update event
        if (controller == controllers[static_cast<size_t>(HidController::NPad)]) {
            continue;
        }
        controller->OnUpdate(core_timing);
    }
}

void ResourceManager::UpdateNpad(std::uintptr_t user_data, std::chrono::nanoseconds ns_late) {
    auto& core_timing = system.CoreTiming();

    controllers[static_cast<size_t>(HidController::NPad)]->OnUpdate(core_timing);
}

void ResourceManager::UpdateMouseKeyboard(std::uintptr_t user_data,
                                          std::chrono::nanoseconds ns_late) {
    auto& core_timing = system.CoreTiming();

    controllers[static_cast<size_t>(HidController::Mouse)]->OnUpdate(core_timing);
    controllers[static_cast<size_t>(HidController::Keyboard)]->OnUpdate(core_timing);
}

void ResourceManager::UpdateMotion(std::uintptr_t user_data, std::chrono::nanoseconds ns_late) {
    auto& core_timing = system.CoreTiming();

    controllers[static_cast<size_t>(HidController::NPad)]->OnMotionUpdate(core_timing);
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
