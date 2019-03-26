// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <string>

namespace Service::SM {
class ServiceManager;
}

namespace Service::Yuzu {

void InstallInterfaces(SM::ServiceManager& sm, std::string data,
                       std::function<void(u32, std::string)> finish_callback);

} // namespace Service::Yuzu
