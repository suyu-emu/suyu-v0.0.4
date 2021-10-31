// SPDX-FileCopyrightText: 2021 yuzu emulator team and Skyline Team and Contributors
// (https://github.com/skyline-emu/)
// SPDX-License-Identifier: GPL-3.0-or-later Licensed under GPLv3
// or any later version Refer to the license.txt file included.

#include <cstdlib>
#include <cstring>

#include "common/assert.h"
#include "common/logging/log.h"
#include "common/scope_exit.h"
#include "core/core.h"
#include "core/hle/kernel/k_event.h"
#include "core/hle/kernel/k_writable_event.h"
#include "core/hle/service/nvdrv/devices/nvhost_ctrl.h"
#include "video_core/gpu.h"

namespace Service::Nvidia::Devices {

nvhost_ctrl::nvhost_ctrl(Core::System& system_, EventInterface& events_interface_,
                         SyncpointManager& syncpoint_manager_)
    : nvdevice{system_}, events_interface{events_interface_}, syncpoint_manager{
                                                                  syncpoint_manager_} {}
nvhost_ctrl::~nvhost_ctrl() = default;

NvResult nvhost_ctrl::Ioctl1(DeviceFD fd, Ioctl command, const std::vector<u8>& input,
                             std::vector<u8>& output) {
    switch (command.group) {
    case 0x0:
        switch (command.cmd) {
        case 0x1b:
            return NvOsGetConfigU32(input, output);
        case 0x1c:
            return IocCtrlClearEventWait(input, output);
        case 0x1d:
            return IocCtrlEventWait(input, output, true);
        case 0x1e:
            return IocCtrlEventWait(input, output, false);
        case 0x1f:
            return IocCtrlEventRegister(input, output);
        case 0x20:
            return IocCtrlEventUnregister(input, output);
        }
        break;
    default:
        break;
    }

    UNIMPLEMENTED_MSG("Unimplemented ioctl={:08X}", command.raw);
    return NvResult::NotImplemented;
}

NvResult nvhost_ctrl::Ioctl2(DeviceFD fd, Ioctl command, const std::vector<u8>& input,
                             const std::vector<u8>& inline_input, std::vector<u8>& output) {
    UNIMPLEMENTED_MSG("Unimplemented ioctl={:08X}", command.raw);
    return NvResult::NotImplemented;
}

NvResult nvhost_ctrl::Ioctl3(DeviceFD fd, Ioctl command, const std::vector<u8>& input,
                             std::vector<u8>& output, std::vector<u8>& inline_outpu) {
    UNIMPLEMENTED_MSG("Unimplemented ioctl={:08X}", command.raw);
    return NvResult::NotImplemented;
}

void nvhost_ctrl::OnOpen(DeviceFD fd) {}
void nvhost_ctrl::OnClose(DeviceFD fd) {}

NvResult nvhost_ctrl::NvOsGetConfigU32(const std::vector<u8>& input, std::vector<u8>& output) {
    IocGetConfigParams params{};
    std::memcpy(&params, input.data(), sizeof(params));
    LOG_TRACE(Service_NVDRV, "called, setting={}!{}", params.domain_str.data(),
              params.param_str.data());
    return NvResult::ConfigVarNotFound; // Returns error on production mode
}

NvResult nvhost_ctrl::IocCtrlEventWait(const std::vector<u8>& input, std::vector<u8>& output,
                                       bool is_allocation) {
    IocCtrlEventWaitParams params{};
    std::memcpy(&params, input.data(), sizeof(params));
    LOG_DEBUG(Service_NVDRV, "syncpt_id={}, threshold={}, timeout={}, is_allocation={}",
              params.fence.id, params.fence.value, params.timeout, is_allocation);

    bool must_unmark_fail = !is_allocation;
    const u32 event_id = params.value.raw;
    SCOPE_EXIT({
        std::memcpy(output.data(), &params, sizeof(params));
        if (must_unmark_fail) {
            events_interface.fails[event_id] = 0;
        }
    });

    const u32 fence_id = static_cast<u32>(params.fence.id);

    if (fence_id >= MaxSyncPoints) {
        return NvResult::BadParameter;
    }

    if (params.fence.value == 0) {
        params.value.raw = syncpoint_manager.GetSyncpointMin(fence_id);
        return NvResult::Success;
    }

    if (syncpoint_manager.IsSyncpointExpired(fence_id, params.fence.value)) {
        params.value.raw = syncpoint_manager.GetSyncpointMin(fence_id);
        return NvResult::Success;
    }

    if (const auto new_value = syncpoint_manager.RefreshSyncpoint(fence_id);
        syncpoint_manager.IsSyncpointExpired(fence_id, params.fence.value)) {
        params.value.raw = new_value;
        return NvResult::Success;
    }

    auto& gpu = system.GPU();
    const u32 target_value = params.fence.value;

    auto lock = events_interface.Lock();

    u32 slot = [&]() {
        if (is_allocation) {
            params.value.raw = 0;
            return events_interface.FindFreeEvent(fence_id);
        } else {
            return params.value.raw;
        }
    }();

    const auto check_failing = [&]() {
        if (events_interface.fails[slot] > 1) {
            {
                auto lk = system.StallProcesses();
                gpu.WaitFence(fence_id, target_value);
                system.UnstallProcesses();
            }
            params.value.raw = target_value;
            return true;
        }
        return false;
    };

    if (params.timeout == 0) {
        if (check_failing()) {
            return NvResult::Success;
        }
        return NvResult::Timeout;
    }

    if (slot >= MaxNvEvents) {
        return NvResult::BadParameter;
    }

    auto* event = events_interface.events[slot];

    if (!event) {
        return NvResult::BadParameter;
    }

    if (events_interface.IsBeingUsed(slot)) {
        return NvResult::BadParameter;
    }

    if (check_failing()) {
        return NvResult::Success;
    }

    params.value.raw = 0;

    events_interface.status[slot].store(EventState::Waiting, std::memory_order_release);
    events_interface.assigned_syncpt[slot] = fence_id;
    events_interface.assigned_value[slot] = target_value;
    if (is_allocation) {
        params.value.syncpoint_id_for_allocation.Assign(static_cast<u16>(fence_id));
        params.value.event_allocated.Assign(1);
    } else {
        params.value.syncpoint_id.Assign(fence_id);
    }
    params.value.raw |= slot;

    gpu.RegisterSyncptInterrupt(fence_id, target_value);
    return NvResult::Timeout;
}

NvResult nvhost_ctrl::FreeEvent(u32 slot) {
    if (slot >= MaxNvEvents) {
        return NvResult::BadParameter;
    }

    if (!events_interface.registered[slot]) {
        return NvResult::Success;
    }

    if (events_interface.IsBeingUsed(slot)) {
        return NvResult::Busy;
    }

    events_interface.Free(slot);
    return NvResult::Success;
}

NvResult nvhost_ctrl::IocCtrlEventRegister(const std::vector<u8>& input, std::vector<u8>& output) {
    IocCtrlEventRegisterParams params{};
    std::memcpy(&params, input.data(), sizeof(params));
    const u32 event_id = params.user_event_id;
    LOG_DEBUG(Service_NVDRV, " called, user_event_id: {:X}", event_id);
    if (event_id >= MaxNvEvents) {
        return NvResult::BadParameter;
    }

    auto lock = events_interface.Lock();

    if (events_interface.registered[event_id]) {
        const auto result = FreeEvent(event_id);
        if (result != NvResult::Success) {
            return result;
        }
    }
    events_interface.Create(event_id);
    return NvResult::Success;
}

NvResult nvhost_ctrl::IocCtrlEventUnregister(const std::vector<u8>& input,
                                             std::vector<u8>& output) {
    IocCtrlEventUnregisterParams params{};
    std::memcpy(&params, input.data(), sizeof(params));
    const u32 event_id = params.user_event_id & 0x00FF;
    LOG_DEBUG(Service_NVDRV, " called, user_event_id: {:X}", event_id);

    auto lock = events_interface.Lock();
    return FreeEvent(event_id);
}

NvResult nvhost_ctrl::IocCtrlClearEventWait(const std::vector<u8>& input, std::vector<u8>& output) {
    IocCtrlEventClearParams params{};
    std::memcpy(&params, input.data(), sizeof(params));

    u32 event_id = params.event_id.slot;
    LOG_DEBUG(Service_NVDRV, "called, event_id: {:X}", event_id);

    if (event_id >= MaxNvEvents) {
        return NvResult::BadParameter;
    }

    auto lock = events_interface.Lock();

    if (events_interface.status[event_id].exchange(
            EventState::Cancelling, std::memory_order_acq_rel) == EventState::Waiting) {
        system.GPU().CancelSyncptInterrupt(events_interface.assigned_syncpt[event_id],
                                           events_interface.assigned_value[event_id]);
        syncpoint_manager.RefreshSyncpoint(events_interface.assigned_syncpt[event_id]);
    }
    events_interface.fails[event_id]++;
    events_interface.status[event_id].store(EventState::Cancelled, std::memory_order_release);
    events_interface.events[event_id]->GetWritableEvent().Clear();

    return NvResult::Success;
}

} // namespace Service::Nvidia::Devices
