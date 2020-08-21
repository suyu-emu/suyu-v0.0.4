// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/core.h"
#include "core/frontend/applets/controller.h"
#include "core/hle/service/hid/controllers/npad.h"
#include "core/hle/service/hid/hid.h"
#include "core/hle/service/sm/sm.h"

namespace Core::Frontend {

ControllerApplet::~ControllerApplet() = default;

DefaultControllerApplet::~DefaultControllerApplet() = default;

void DefaultControllerApplet::ReconfigureControllers(std::function<void()> callback,
                                                     ControllerParameters parameters) const {
    LOG_INFO(Service_HID, "called, deducing the best configuration based on the given parameters!");

    auto& npad =
        Core::System::GetInstance()
            .ServiceManager()
            .GetService<Service::HID::Hid>("hid")
            ->GetAppletResource()
            ->GetController<Service::HID::Controller_NPad>(Service::HID::HidController::NPad);

    auto& players = Settings::values.players;

    // Deduce the best configuration based on the input parameters.
    for (std::size_t index = 0; index < players.size(); ++index) {
        // First, disconnect all controllers regardless of the value of keep_controllers_connected.
        // This makes it easy to connect the desired controllers.
        npad.DisconnectNPadAtIndex(index);
    }

    callback();
}

} // namespace Core::Frontend
