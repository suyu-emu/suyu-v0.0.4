// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/scope_exit.h"

#include "core/hle/service/am/am_results.h"
#include "core/hle/service/am/applet.h"

namespace Service::AM {

AppletStorageChannel::AppletStorageChannel(KernelHelpers::ServiceContext& context)
    : m_event(context) {}
AppletStorageChannel::~AppletStorageChannel() = default;

void AppletStorageChannel::PushData(std::shared_ptr<IStorage> storage) {
    std::scoped_lock lk{m_lock};

    m_data.emplace_back(std::move(storage));
    m_event.Signal();
}

Result AppletStorageChannel::PopData(std::shared_ptr<IStorage>* out_storage) {
    std::scoped_lock lk{m_lock};

    SCOPE_EXIT({
        if (m_data.empty()) {
            m_event.Clear();
        }
    });

    R_UNLESS(!m_data.empty(), AM::ResultNoDataInChannel);

    *out_storage = std::move(m_data.front());
    m_data.pop_front();

    R_SUCCEED();
}

Kernel::KReadableEvent* AppletStorageChannel::GetEvent() {
    return m_event.GetHandle();
}

AppletStorageHolder::AppletStorageHolder(Core::System& system)
    : context(system, "AppletStorageHolder"), in_data(context), interactive_in_data(context),
      out_data(context), interactive_out_data(context), state_changed_event(context) {}

AppletStorageHolder::~AppletStorageHolder() = default;

Applet::Applet(Core::System& system, std::unique_ptr<Process> process_)
    : context(system, "Applet"), message_queue(system), process(std::move(process_)),
      hid_registration(system, *process), gpu_error_detected_event(context),
      friend_invitation_storage_channel_event(context), notification_storage_channel_event(context),
      health_warning_disappeared_system_event(context), acquired_sleep_lock_event(context),
      pop_from_general_channel_event(context), library_applet_launchable_event(context),
      accumulated_suspended_tick_changed_event(context), sleep_lock_event(context) {

    aruid = process->GetProcessId();
    program_id = process->GetProgramId();
}

Applet::~Applet() = default;

} // namespace Service::AM
