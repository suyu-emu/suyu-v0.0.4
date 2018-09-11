// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service::Audio {

class AudDbg final : public ServiceFramework<AudDbg> {
public:
    explicit AudDbg(const char* name);
    ~AudDbg() override;
};

} // namespace Service::Audio
