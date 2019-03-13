// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Kernel {
class HLERequestContext;
}

namespace Service::NVFlinger {
class NVFlinger;
}

namespace Service::VI {

class VI_U final : public ServiceFramework<VI_U> {
public:
    explicit VI_U(std::shared_ptr<NVFlinger::NVFlinger> nv_flinger);
    ~VI_U() override;

private:
    void GetDisplayService(Kernel::HLERequestContext& ctx);

    std::shared_ptr<NVFlinger::NVFlinger> nv_flinger;
};

} // namespace Service::VI
