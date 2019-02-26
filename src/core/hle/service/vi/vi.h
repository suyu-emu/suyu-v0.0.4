// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "common/common_types.h"

namespace Kernel {
class HLERequestContext;
}

namespace Service::NVFlinger {
class NVFlinger;
}

namespace Service::SM {
class ServiceManager;
}

namespace Service::VI {
namespace detail {
void GetDisplayServiceImpl(Kernel::HLERequestContext& ctx,
                           std::shared_ptr<NVFlinger::NVFlinger> nv_flinger);
}

enum class DisplayResolution : u32 {
    DockedWidth = 1920,
    DockedHeight = 1080,
    UndockedWidth = 1280,
    UndockedHeight = 720,
};

/// Registers all VI services with the specified service manager.
void InstallInterfaces(SM::ServiceManager& service_manager,
                       std::shared_ptr<NVFlinger::NVFlinger> nv_flinger);

} // namespace Service::VI
