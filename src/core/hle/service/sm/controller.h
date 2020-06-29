// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service::SM {

class Controller final : public ServiceFramework<Controller> {
public:
    Controller();
    ~Controller() override;

private:
    void ConvertCurrentObjectToDomain(Kernel::HLERequestContext& ctx);
    void CloneCurrentObject(Kernel::HLERequestContext& ctx);
    void CloneCurrentObjectEx(Kernel::HLERequestContext& ctx);
    void QueryPointerBufferSize(Kernel::HLERequestContext& ctx);
};

} // namespace Service::SM
