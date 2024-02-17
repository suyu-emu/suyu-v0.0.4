// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

namespace Core {
class System;
}

namespace Service::Nvnflinger {
class HosBinderDriverServer;
class Nvnflinger;
} // namespace Service::Nvnflinger

namespace Service::VI {

void LoopProcess(Core::System& system, Nvnflinger::Nvnflinger& nvnflinger,
                 Nvnflinger::HosBinderDriverServer& hos_binder_driver_server);

} // namespace Service::VI
