// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/bcat/module.h"

namespace Service::BCAT {

class BCAT final : public Module::Interface {
public:
    explicit BCAT(std::shared_ptr<Module> module, const char* name);
    ~BCAT() override;
};

} // namespace Service::BCAT
