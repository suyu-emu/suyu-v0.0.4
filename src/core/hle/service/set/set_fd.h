// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service::Set {

class SET_FD final : public ServiceFramework<SET_FD> {
public:
    explicit SET_FD();
    ~SET_FD() override;
};

} // namespace Service::Set
