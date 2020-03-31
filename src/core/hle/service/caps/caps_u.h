// Copyright 2020 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Kernel {
class HLERequestContext;
}

namespace Service::Capture {

class CAPS_U final : public ServiceFramework<CAPS_U> {
public:
    explicit CAPS_U();
    ~CAPS_U() override;

private:
    void GetAlbumContentsFileListForApplication(Kernel::HLERequestContext& ctx);
};

} // namespace Service::Capture
