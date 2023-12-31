// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/service.h"

namespace Service::AM {

class IAppletCommonFunctions final : public ServiceFramework<IAppletCommonFunctions> {
public:
    explicit IAppletCommonFunctions(Core::System& system_);
    ~IAppletCommonFunctions() override;

private:
    void SetCpuBoostRequestPriority(HLERequestContext& ctx);
};

} // namespace Service::AM
