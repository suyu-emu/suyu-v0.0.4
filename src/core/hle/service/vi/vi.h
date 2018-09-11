// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/nvflinger/nvflinger.h"
#include "core/hle/service/service.h"

namespace CoreTiming {
struct EventType;
}

namespace Service::VI {

enum class DisplayResolution : u32 {
    DockedWidth = 1920,
    DockedHeight = 1080,
    UndockedWidth = 1280,
    UndockedHeight = 720,
};

class Module final {
public:
    class Interface : public ServiceFramework<Interface> {
    public:
        explicit Interface(std::shared_ptr<Module> module, const char* name,
                           std::shared_ptr<NVFlinger::NVFlinger> nv_flinger);
        ~Interface() override;

        void GetDisplayService(Kernel::HLERequestContext& ctx);

    protected:
        std::shared_ptr<Module> module;
        std::shared_ptr<NVFlinger::NVFlinger> nv_flinger;
    };
};

/// Registers all VI services with the specified service manager.
void InstallInterfaces(SM::ServiceManager& service_manager,
                       std::shared_ptr<NVFlinger::NVFlinger> nv_flinger);

} // namespace Service::VI
