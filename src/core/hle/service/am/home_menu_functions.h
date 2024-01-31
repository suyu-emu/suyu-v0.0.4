// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/kernel_helpers.h"
#include "core/hle/service/service.h"

namespace Service::AM {

class IHomeMenuFunctions final : public ServiceFramework<IHomeMenuFunctions> {
public:
    explicit IHomeMenuFunctions(Core::System& system_);
    ~IHomeMenuFunctions() override;

private:
    void RequestToGetForeground(HLERequestContext& ctx);
    void GetPopFromGeneralChannelEvent(HLERequestContext& ctx);

    KernelHelpers::ServiceContext service_context;

    Kernel::KEvent* pop_from_general_channel_event;
};

} // namespace Service::AM
