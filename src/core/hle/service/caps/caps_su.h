// Copyright 2020 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Kernel {
class HLERequestContext;
}

namespace Service::Capture {

class CAPS_SU final : public ServiceFramework<CAPS_SU> {
public:
    explicit CAPS_SU();
    ~CAPS_SU() override;
};

} // namespace Service::Capture
