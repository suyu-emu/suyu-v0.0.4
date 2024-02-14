// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include "common/common_types.h"

namespace Core {
class System;
}

namespace Service::Nvnflinger {
class IHOSBinderDriver;
} // namespace Service::Nvnflinger

union Result;

namespace Service::VI {

class FbshareBufferManager;
class IApplicationDisplayService;
enum class Permission;
enum class Policy : u32;

Result GetApplicationDisplayService(
    std::shared_ptr<IApplicationDisplayService>* out_application_display_service,
    Core::System& system, std::shared_ptr<Nvnflinger::IHOSBinderDriver> binder_service,
    std::shared_ptr<FbshareBufferManager> shared_buffer_manager, Permission permission,
    Policy policy);

} // namespace Service::VI
