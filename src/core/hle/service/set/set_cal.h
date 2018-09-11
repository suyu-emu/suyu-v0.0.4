// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service::Set {

class SET_CAL final : public ServiceFramework<SET_CAL> {
public:
    explicit SET_CAL();
    ~SET_CAL();
};

} // namespace Service::Set
