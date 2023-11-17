// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <chrono>

#include "core/core.h"
#include "core/hle/service/hid/controllers/controller_base.h"
#include "core/hle/service/kernel_helpers.h"
#include "core/hle/service/service.h"

namespace Core::Timing {
struct EventType;
}

namespace Core::HID {
class HIDCore;
}

namespace Service::HID {

enum class HidController : std::size_t {
    DebugPad,
    Touchscreen,
    Mouse,
    Keyboard,
    XPad,
    HomeButton,
    SleepButton,
    CaptureButton,
    InputDetector,
    UniquePad,
    NPad,
    Gesture,
    ConsoleSixAxisSensor,
    DebugMouse,
    Palma,

    MaxControllers,
};
class ResourceManager {
public:
    explicit ResourceManager(Core::System& system_);
    ~ResourceManager();

    template <typename T>
    T& GetController(HidController controller) {
        return static_cast<T&>(*controllers[static_cast<size_t>(controller)]);
    }

    template <typename T>
    const T& GetController(HidController controller) const {
        return static_cast<T&>(*controllers[static_cast<size_t>(controller)]);
    }

    void Initialize();

    void UpdateControllers(std::uintptr_t user_data, std::chrono::nanoseconds ns_late);
    void UpdateNpad(std::uintptr_t user_data, std::chrono::nanoseconds ns_late);
    void UpdateMouseKeyboard(std::uintptr_t user_data, std::chrono::nanoseconds ns_late);
    void UpdateMotion(std::uintptr_t user_data, std::chrono::nanoseconds ns_late);

private:
    template <typename T>
    void MakeController(HidController controller, u8* shared_memory) {
        if constexpr (std::is_constructible_v<T, Core::System&, u8*>) {
            controllers[static_cast<std::size_t>(controller)] =
                std::make_unique<T>(system, shared_memory);
        } else {
            controllers[static_cast<std::size_t>(controller)] =
                std::make_unique<T>(system.HIDCore(), shared_memory);
        }
    }

    template <typename T>
    void MakeControllerWithServiceContext(HidController controller, u8* shared_memory) {
        controllers[static_cast<std::size_t>(controller)] =
            std::make_unique<T>(system.HIDCore(), shared_memory, service_context);
    }

    bool is_initialized{false};
    std::array<std::unique_ptr<ControllerBase>, static_cast<size_t>(HidController::MaxControllers)>
        controllers{};

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
