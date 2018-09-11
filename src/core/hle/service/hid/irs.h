// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service::HID {

class IRS final : public ServiceFramework<IRS> {
public:
    explicit IRS();
    ~IRS() override;
};

class IRS_SYS final : public ServiceFramework<IRS_SYS> {
public:
    explicit IRS_SYS();
    ~IRS_SYS() override;
};

} // namespace Service::HID
