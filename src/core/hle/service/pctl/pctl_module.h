// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/pctl/pctl_types.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::PCTL {

class Module final {
public:
    class Interface : public ServiceFramework<Interface> {
    public:
        explicit Interface(Core::System& system_, std::shared_ptr<Module> module_,
                           const char* name_, Capability capability_);
        ~Interface() override;

        void CreateService(HLERequestContext& ctx);
        void CreateServiceWithoutInitialize(HLERequestContext& ctx);

    protected:
        std::shared_ptr<Module> module;

    private:
        Capability capability{};
    };
};

void LoopProcess(Core::System& system);

} // namespace Service::PCTL
