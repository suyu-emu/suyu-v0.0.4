// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service::HID {

class XCD_SYS final : public ServiceFramework<XCD_SYS> {
public:
    explicit XCD_SYS();
    ~XCD_SYS() override;
};

} // namespace Service::HID
