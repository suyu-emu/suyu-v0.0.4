// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/am/am.h"
#include "core/hle/service/am/applet_ae.h"
#include "core/hle/service/am/applet_oe.h"
#include "core/hle/service/am/idle.h"
#include "core/hle/service/am/omm.h"
#include "core/hle/service/am/spsm.h"
#include "core/hle/service/server_manager.h"

namespace Service::AM {

void LoopProcess(Nvnflinger::Nvnflinger& nvnflinger, Core::System& system) {
    auto server_manager = std::make_unique<ServerManager>(system);

    server_manager->RegisterNamedService("appletAE",
                                         std::make_shared<AppletAE>(nvnflinger, system));
    server_manager->RegisterNamedService("appletOE",
                                         std::make_shared<AppletOE>(nvnflinger, system));
    server_manager->RegisterNamedService("idle:sys", std::make_shared<IdleSys>(system));
    server_manager->RegisterNamedService("omm", std::make_shared<OMM>(system));
    server_manager->RegisterNamedService("spsm", std::make_shared<SPSM>(system));
    ServerManager::RunServer(std::move(server_manager));
}

} // namespace Service::AM
