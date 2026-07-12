// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/cmif_types.h"
#include "core/hle/service/service.h"

namespace Service::AM {

class ISystemProcessCommonFunctions final
    : public ServiceFramework<ISystemProcessCommonFunctions> {
public:
    explicit ISystemProcessCommonFunctions(Core::System& system_);
    ~ISystemProcessCommonFunctions() override;
};

} // namespace Service::AM
