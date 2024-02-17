// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::NS {

class IApplicationManagerInterface;

class NS final : public ServiceFramework<NS> {
public:
    explicit NS(const char* name, Core::System& system_);
    ~NS() override;

    std::shared_ptr<IApplicationManagerInterface> GetApplicationManagerInterface() const;

private:
    template <typename T, typename... Args>
    void PushInterface(HLERequestContext& ctx);

    void PushIApplicationManagerInterface(HLERequestContext& ctx);

    template <typename T, typename... Args>
    std::shared_ptr<T> GetInterface(Args&&... args) const;
};

void LoopProcess(Core::System& system);

} // namespace Service::NS
