// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/hle/service/cmif_types.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::BCAT {

class INewsDatabaseService final : public ServiceFramework<INewsDatabaseService> {
public:
    explicit INewsDatabaseService(Core::System& system_);
    ~INewsDatabaseService() override;

private:
    Result Count(Out<u32> out_count, InBuffer<BufferAttr_HipcPointer> buffer_data);
};

} // namespace Service::BCAT
