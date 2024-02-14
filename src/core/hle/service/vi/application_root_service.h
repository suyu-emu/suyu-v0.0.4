// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/cmif_types.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::Nvnflinger {
class HosBinderDriverServer;
class Nvnflinger;
} // namespace Service::Nvnflinger

namespace Service::VI {

class IApplicationDisplayService;
enum class Policy : u32;

class IApplicationRootService final : public ServiceFramework<IApplicationRootService> {
public:
    explicit IApplicationRootService(Core::System& system_, Nvnflinger::Nvnflinger& nvnflinger,
                                     Nvnflinger::HosBinderDriverServer& hos_binder_driver_server);
    ~IApplicationRootService() override;

private:
    Result GetDisplayService(
        Out<SharedPointer<IApplicationDisplayService>> out_application_display_service,
        Policy policy);

private:
    Nvnflinger::Nvnflinger& m_nvnflinger;
    Nvnflinger::HosBinderDriverServer& m_hos_binder_driver_server;
};

} // namespace Service::VI
