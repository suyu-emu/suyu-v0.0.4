// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::Mii {
class MiiManager;

class MiiDBModule final : public ServiceFramework<MiiDBModule> {
public:
    explicit MiiDBModule(Core::System& system_, const char* name_,
                         std::shared_ptr<MiiManager> mii_manager, bool is_system_);
    ~MiiDBModule() override;

    std::shared_ptr<MiiManager> GetMiiManager();

private:
    void GetDatabaseService(HLERequestContext& ctx);

    std::shared_ptr<MiiManager> manager = nullptr;
    bool is_system{};
};

void LoopProcess(Core::System& system);

} // namespace Service::Mii
