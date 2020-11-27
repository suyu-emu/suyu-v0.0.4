// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Kernel {
class WritableEvent;
}

namespace Service::AOC {

class AOC_U final : public ServiceFramework<AOC_U> {
public:
    explicit AOC_U(Core::System& system);
    ~AOC_U() override;

private:
    void CountAddOnContent(Kernel::HLERequestContext& ctx);
    void ListAddOnContent(Kernel::HLERequestContext& ctx);
    void GetAddOnContentBaseId(Kernel::HLERequestContext& ctx);
    void PrepareAddOnContent(Kernel::HLERequestContext& ctx);
    void GetAddOnContentListChangedEvent(Kernel::HLERequestContext& ctx);

    std::vector<u64> add_on_content;
    Kernel::EventPair aoc_change_event;
};

/// Registers all AOC services with the specified service manager.
void InstallInterfaces(SM::ServiceManager& service_manager, Core::System& system);

} // namespace Service::AOC
