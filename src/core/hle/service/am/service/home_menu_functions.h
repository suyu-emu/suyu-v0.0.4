// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/cmif_types.h"
#include "core/hle/service/kernel_helpers.h"
#include "core/hle/service/os/event.h"
#include "core/hle/service/service.h"

namespace Service::AM {

struct Applet;

class IHomeMenuFunctions final : public ServiceFramework<IHomeMenuFunctions> {
public:
    explicit IHomeMenuFunctions(Core::System& system_, std::shared_ptr<Applet> applet);
    ~IHomeMenuFunctions() override;

private:
    Result RequestToGetForeground();
    Result LockForeground();
    Result UnlockForeground();
    Result GetPopFromGeneralChannelEvent(OutCopyHandle<Kernel::KReadableEvent> out_event);
    Result IsRebootEnabled(Out<bool> out_is_reboot_enbaled);
    Result IsForceTerminateApplicationDisabledForDebug(
        Out<bool> out_is_force_terminate_application_disabled_for_debug);

    const std::shared_ptr<Applet> m_applet;
    KernelHelpers::ServiceContext m_context;
    Event m_pop_from_general_channel_event;
};

} // namespace Service::AM
