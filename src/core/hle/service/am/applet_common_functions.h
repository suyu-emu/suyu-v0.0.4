// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/service.h"

namespace Service::AM {

struct Applet;

class IAppletCommonFunctions final : public ServiceFramework<IAppletCommonFunctions> {
public:
    explicit IAppletCommonFunctions(Core::System& system_, std::shared_ptr<Applet> applet_);
    ~IAppletCommonFunctions() override;

private:
    void SetCpuBoostRequestPriority(HLERequestContext& ctx);
    void GetCurrentApplicationId(HLERequestContext& ctx);

    const std::shared_ptr<Applet> applet;
};

} // namespace Service::AM
