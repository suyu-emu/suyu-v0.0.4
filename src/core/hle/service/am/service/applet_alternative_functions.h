// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/cmif_types.h"
#include "core/hle/service/service.h"

namespace Service::AM {

class IAppletAlternativeFunctions final : public ServiceFramework<IAppletAlternativeFunctions> {
public:
    explicit IAppletAlternativeFunctions(Core::System& system_);
    ~IAppletAlternativeFunctions() override;
};

} // namespace Service::AM
