// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/hid/hid.h"
#include "core/hle/service/hid/hid_debug_server.h"
#include "core/hle/service/hid/hid_server.h"
#include "core/hle/service/hid/hid_system_server.h"
#include "core/hle/service/hid/hidbus.h"
#include "core/hle/service/hid/irs.h"
#include "core/hle/service/hid/resource_manager.h"
#include "core/hle/service/hid/xcd.h"
#include "core/hle/service/server_manager.h"

namespace Service::HID {

void LoopProcess(Core::System& system) {
    auto server_manager = std::make_unique<ServerManager>(system);
    std::shared_ptr<ResourceManager> resouce_manager = std::make_shared<ResourceManager>(system);

    server_manager->RegisterNamedService("hid",
                                         std::make_shared<IHidServer>(system, resouce_manager));
    server_manager->RegisterNamedService(
        "hid:dbg", std::make_shared<IHidDebugServer>(system, resouce_manager));
    server_manager->RegisterNamedService(
        "hid:sys", std::make_shared<IHidSystemServer>(system, resouce_manager));

    server_manager->RegisterNamedService("hidbus", std::make_shared<HidBus>(system));

    server_manager->RegisterNamedService("irs", std::make_shared<IRS::IRS>(system));
    server_manager->RegisterNamedService("irs:sys", std::make_shared<IRS::IRS_SYS>(system));

    server_manager->RegisterNamedService("xcd:sys", std::make_shared<XCD_SYS>(system));

    system.RunServer(std::move(server_manager));
}

} // namespace Service::HID
