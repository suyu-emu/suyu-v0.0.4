// Copyright 2020 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Kernel {
class HLERequestContext;
}

namespace Service::Capture {

class CAPS_SS final : public ServiceFramework<CAPS_SS> {
public:
    explicit CAPS_SS();
    ~CAPS_SS() override;
};

} // namespace Service::Capture
