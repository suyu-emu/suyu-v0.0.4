// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

namespace Service::SM {
class ServiceManager;
}

namespace Service::Audio {

/// Registers all Audio services with the specified service manager.
void InstallInterfaces(SM::ServiceManager& service_manager);

} // namespace Service::Audio
