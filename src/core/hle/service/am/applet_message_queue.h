// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <queue>

#include "core/hle/service/am/am_types.h"
#include "core/hle/service/kernel_helpers.h"
#include "core/hle/service/service.h"

namespace Kernel {
class KReadableEvent;
} // namespace Kernel

namespace Service::AM {

class AppletMessageQueue {
public:
    explicit AppletMessageQueue(Core::System& system);
    ~AppletMessageQueue();

    Kernel::KReadableEvent& GetMessageReceiveEvent();
    Kernel::KReadableEvent& GetOperationModeChangedEvent();
    void PushMessage(AppletMessage msg);
    AppletMessage PopMessage();
    std::size_t GetMessageCount() const;
    void RequestExit();
    void RequestResume();
    void FocusStateChanged();
    void OperationModeChanged();

private:
    KernelHelpers::ServiceContext service_context;

    Kernel::KEvent* on_new_message;
    Kernel::KEvent* on_operation_mode_changed;

    mutable std::mutex lock;
    std::queue<AppletMessage> messages;
};

} // namespace Service::AM
