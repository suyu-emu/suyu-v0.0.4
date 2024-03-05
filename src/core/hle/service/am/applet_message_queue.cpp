// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/am/applet_message_queue.h"
#include "core/hle/service/ipc_helpers.h"

namespace Service::AM {

AppletMessageQueue::AppletMessageQueue(Core::System& system)
    : service_context{system, "AppletMessageQueue"} {
    on_new_message = service_context.CreateEvent("AMMessageQueue:OnMessageReceived");
    on_operation_mode_changed = service_context.CreateEvent("AMMessageQueue:OperationModeChanged");
}

AppletMessageQueue::~AppletMessageQueue() {
    service_context.CloseEvent(on_new_message);
    service_context.CloseEvent(on_operation_mode_changed);
}

Kernel::KReadableEvent& AppletMessageQueue::GetMessageReceiveEvent() {
    return on_new_message->GetReadableEvent();
}

Kernel::KReadableEvent& AppletMessageQueue::GetOperationModeChangedEvent() {
    return on_operation_mode_changed->GetReadableEvent();
}

void AppletMessageQueue::PushMessage(AppletMessage msg) {
    {
        std::scoped_lock lk{lock};
        messages.push(msg);
    }
    on_new_message->Signal();
}

AppletMessage AppletMessageQueue::PopMessage() {
    std::scoped_lock lk{lock};
    if (messages.empty()) {
        on_new_message->Clear();
        return AppletMessage::None;
    }
    auto msg = messages.front();
    messages.pop();
    if (messages.empty()) {
        on_new_message->Clear();
    }
    return msg;
}

std::size_t AppletMessageQueue::GetMessageCount() const {
    std::scoped_lock lk{lock};
    return messages.size();
}

void AppletMessageQueue::RequestExit() {
    PushMessage(AppletMessage::Exit);
}

void AppletMessageQueue::RequestResume() {
    PushMessage(AppletMessage::Resume);
}

void AppletMessageQueue::FocusStateChanged() {
    PushMessage(AppletMessage::FocusStateChanged);
}

void AppletMessageQueue::OperationModeChanged() {
    PushMessage(AppletMessage::OperationModeChanged);
    PushMessage(AppletMessage::PerformanceModeChanged);
    on_operation_mode_changed->Signal();
}

} // namespace Service::AM
