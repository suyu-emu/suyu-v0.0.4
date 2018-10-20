// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once
#include "core/hle/service/service.h"

namespace Service::SM {
class ServiceManager;
}

namespace Service::PSM {

class PSM final : public ServiceFramework<PSM> {
public:
    explicit PSM();
    ~PSM() override;
};

void InstallInterfaces(SM::ServiceManager& sm);

} // namespace Service::PSM
