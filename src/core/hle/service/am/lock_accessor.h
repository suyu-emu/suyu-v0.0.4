// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/kernel_helpers.h"
#include "core/hle/service/service.h"

namespace Service::AM {

class ILockAccessor final : public ServiceFramework<ILockAccessor> {
public:
    explicit ILockAccessor(Core::System& system_);
    ~ILockAccessor() override;

private:
    void TryLock(HLERequestContext& ctx);
    void Unlock(HLERequestContext& ctx);
    void GetEvent(HLERequestContext& ctx);
    void IsLocked(HLERequestContext& ctx);

    bool is_locked{};

    Kernel::KEvent* lock_event;
    KernelHelpers::ServiceContext service_context;
};

} // namespace Service::AM
