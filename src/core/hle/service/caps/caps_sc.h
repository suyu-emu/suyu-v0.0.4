// Copyright 2020 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Kernel {
class HLERequestContext;
}

namespace Service::Capture {

class CAPS_SC final : public ServiceFramework<CAPS_SC> {
public:
    explicit CAPS_SC();
    ~CAPS_SC() override;
};

} // namespace Service::Capture
