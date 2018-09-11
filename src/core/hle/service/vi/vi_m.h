// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/vi/vi.h"

namespace Service::VI {

class VI_M final : public Module::Interface {
public:
    explicit VI_M(std::shared_ptr<Module> module, std::shared_ptr<NVFlinger::NVFlinger> nv_flinger);
    ~VI_M() override;
};

} // namespace Service::VI
