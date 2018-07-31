// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service::Audio {

class AudRecA final : public ServiceFramework<AudRecA> {
public:
    explicit AudRecA();
};

} // namespace Service::Audio
