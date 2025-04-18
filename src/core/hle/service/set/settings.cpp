// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/server_manager.h"
#include "core/hle/service/set/factory_settings_server.h"
#include "core/hle/service/set/firmware_debug_settings_server.h"
#include "core/hle/service/set/settings.h"
#include "core/hle/service/set/settings_server.h"
#include "core/hle/service/set/system_settings_server.h"

namespace Service::Set {

void LoopProcess(Core::System& system) {
    auto server_manager = std::make_unique<ServerManager>(system);

    server_manager->RegisterNamedService("set", std::make_shared<ISettingsServer>(system));
    server_manager->RegisterNamedService("set:cal",
                                         std::make_shared<IFactorySettingsServer>(system));
    server_manager->RegisterNamedService("set:fd",
                                         std::make_shared<IFirmwareDebugSettingsServer>(system));
    server_manager->RegisterNamedService("set:sys",
                                         std::make_shared<ISystemSettingsServer>(system));
    ServerManager::RunServer(std::move(server_manager));
}

bool IsFirmwareVersionSupported(u32 version) {
    // Add support for firmware version 18.0.0
    return version <= 180000; // 18.0.0 = 180000
}

} // namespace Service::Set
