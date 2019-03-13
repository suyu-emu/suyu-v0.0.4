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

class VI_S final : public ServiceFramework<VI_S> {
public:
    explicit VI_S(std::shared_ptr<NVFlinger::NVFlinger> nv_flinger);
    ~VI_S() override;

private:
    void GetDisplayService(Kernel::HLERequestContext& ctx);

    std::shared_ptr<NVFlinger::NVFlinger> nv_flinger;
};

} // namespace Service::VI
