// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include "common/common_types.h"

namespace Core {
class System;
}

namespace Service::Nvnflinger {
class HosBinderDriverServer;
class Nvnflinger;
} // namespace Service::Nvnflinger

union Result;

namespace Service::VI {

class IApplicationDisplayService;
enum class Permission;
enum class Policy : u32;

Result GetApplicationDisplayService(
    std::shared_ptr<IApplicationDisplayService>* out_application_display_service,
    Core::System& system, Nvnflinger::Nvnflinger& nvnflinger,
    Nvnflinger::HosBinderDriverServer& hos_binder_driver_server, Permission permission,
    Policy policy);

} // namespace Service::VI
