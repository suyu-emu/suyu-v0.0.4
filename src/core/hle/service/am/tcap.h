// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service::AM {

class TCAP final : public ServiceFramework<TCAP> {
public:
    explicit TCAP();
    ~TCAP() override;
};

} // namespace Service::AM
