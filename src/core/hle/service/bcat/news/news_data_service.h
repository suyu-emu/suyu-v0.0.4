// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::BCAT {

class INewsDataService final : public ServiceFramework<INewsDataService> {
public:
    explicit INewsDataService(Core::System& system_);
    ~INewsDataService() override;
};

} // namespace Service::BCAT
