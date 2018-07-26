// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service::Sockets {

class ETHC_C final : public ServiceFramework<ETHC_C> {
public:
    explicit ETHC_C();
};

class ETHC_I final : public ServiceFramework<ETHC_I> {
public:
    explicit ETHC_I();
};

} // namespace Service::Sockets
