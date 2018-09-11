// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Kernel {
class HLERequestContext;
}

namespace Service::Audio {

class AudInU final : public ServiceFramework<AudInU> {
public:
    explicit AudInU();
    ~AudInU() override;
};

} // namespace Service::Audio
