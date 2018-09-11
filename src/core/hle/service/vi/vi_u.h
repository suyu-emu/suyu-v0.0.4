// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/vi/vi.h"

namespace Service::VI {

class VI_U final : public Module::Interface {
public:
    explicit VI_U(std::shared_ptr<Module> module, std::shared_ptr<NVFlinger::NVFlinger> nv_flinger);
    ~VI_U() override;
};

} // namespace Service::VI
