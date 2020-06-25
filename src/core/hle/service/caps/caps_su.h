// Copyright 2020 yuzu Emulator Project
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

private:
    void SetShimLibraryVersion(Kernel::HLERequestContext& ctx);
};

} // namespace Service::Capture
