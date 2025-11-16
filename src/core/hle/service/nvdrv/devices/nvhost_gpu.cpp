// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstring>
#include "common/assert.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/kernel/k_event.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/service/nvdrv/core/container.h"
#include "core/hle/service/nvdrv/core/nvmap.h"
#include "core/hle/service/nvdrv/core/syncpoint_manager.h"
#include "core/hle/service/nvdrv/devices/ioctl_serialization.h"
#include "core/hle/service/nvdrv/devices/nvhost_gpu.h"
#include "core/hle/service/nvdrv/nvdrv.h"
#include "core/memory.h"
#include "video_core/control/channel_state.h"
#include "video_core/engines/puller.h"
#include "video_core/gpu.h"
#include "video_core/host1x/host1x.h"

namespace Service::Nvidia::Devices {
namespace {
Tegra::CommandHeader BuildFenceAction(Tegra::Engines::Puller::FenceOperation op, u32 syncpoint_id) {
    Tegra::Engines::Puller::FenceAction result{};
    result.op.Assign(op);
    result.syncpoint_id.Assign(syncpoint_id);
    return {result.raw};
}
} // namespace

nvhost_gpu::nvhost_gpu(Core::System& system_, EventInterface& events_interface_,
                       NvCore::Container& core_)
    : nvdevice{system_}, events_interface{events_interface_}, core{core_},
      syncpoint_manager{core_.GetSyncpointManager()}, nvmap{core.GetNvMapFile()},
      channel_state{system.GPU().AllocateChannel()} {
    channel_syncpoint = syncpoint_manager.AllocateSyncpoint(false);
    sm_exception_breakpoint_int_report_event =
        events_interface.CreateEvent("GpuChannelSMExceptionBreakpointInt");
    sm_exception_breakpoint_pause_report_event =
        events_interface.CreateEvent("GpuChannelSMExceptionBreakpointPause");
    error_notifier_event = events_interface.CreateEvent("GpuChannelErrorNotifier");
}

nvhost_gpu::~nvhost_gpu() {
    events_interface.FreeEvent(sm_exception_breakpoint_int_report_event);
    events_interface.FreeEvent(sm_exception_breakpoint_pause_report_event);
    events_interface.FreeEvent(error_notifier_event);
    syncpoint_manager.FreeSyncpoint(channel_syncpoint);
}

NvResult nvhost_gpu::Ioctl1(DeviceFD fd, Ioctl command, std::span<const u8> input,
                            std::span<u8> output) {
    switch (command.group) {
    case 0x0:
        switch (command.cmd) {
        case 0x3:
            return WrapFixed(this, &nvhost_gpu::GetWaitbase, input, output);
        default:
            break;
        }
        break;
    case 'H':
        switch (command.cmd) {
        case 0x1:
            return WrapFixed(this, &nvhost_gpu::SetNVMAPfd, input, output);
        case 0x3:
            return WrapFixed(this, &nvhost_gpu::ChannelSetTimeout, input, output);
        case 0x8:
            return WrapFixedVariable(this, &nvhost_gpu::SubmitGPFIFOBase1, input, output, false);
        case 0x9:
            return WrapFixed(this, &nvhost_gpu::AllocateObjectContext, input, output);
        case 0xb:
            return WrapFixed(this, &nvhost_gpu::ZCullBind, input, output);
        case 0xc:
            return WrapFixed(this, &nvhost_gpu::SetErrorNotifier, input, output);
        case 0xd:
            return WrapFixed(this, &nvhost_gpu::SetChannelPriority, input, output);
        case 0x18:
            return WrapFixed(this, &nvhost_gpu::AllocGPFIFOEx, input, output, fd);
        case 0x1a:
            return WrapFixed(this, &nvhost_gpu::AllocGPFIFOEx2, input, output, fd);
        case 0x1b:
            return WrapFixedVariable(this, &nvhost_gpu::SubmitGPFIFOBase1, input, output, true);
        case 0x1d:
            return WrapFixed(this, &nvhost_gpu::ChannelSetTimeslice, input, output);
        default:
            break;
        }
        break;
    case 'G':
        switch (command.cmd) {
        case 0x14:
            return WrapFixed(this, &nvhost_gpu::SetClientData, input, output);
        case 0x15:
            return WrapFixed(this, &nvhost_gpu::GetClientData, input, output);
        default:
            break;
        }
        break;
    }
    UNIMPLEMENTED_MSG("Unimplemented ioctl={:08X}", command.raw);
    return NvResult::NotImplemented;
};

NvResult nvhost_gpu::Ioctl2(DeviceFD fd, Ioctl command, std::span<const u8> input,
                            std::span<const u8> inline_input, std::span<u8> output) {
    switch (command.group) {
    case 'H':
        switch (command.cmd) {
        case 0x1b:
            return WrapFixedInlIn(this, &nvhost_gpu::SubmitGPFIFOBase2, input, inline_input,
                                  output);
        }
        break;
    }
    UNIMPLEMENTED_MSG("Unimplemented ioctl={:08X}", command.raw);
    return NvResult::NotImplemented;
}

NvResult nvhost_gpu::Ioctl3(DeviceFD fd, Ioctl command, std::span<const u8> input,
                            std::span<u8> output, std::span<u8> inline_output) {
    UNIMPLEMENTED_MSG("Unimplemented ioctl={:08X}", command.raw);
    return NvResult::NotImplemented;
}

void nvhost_gpu::OnOpen(NvCore::SessionId session_id, DeviceFD fd) {
    sessions[fd] = session_id;
}

void nvhost_gpu::OnClose(DeviceFD fd) {
    sessions.erase(fd);
}

NvResult nvhost_gpu::SetNVMAPfd(IoctlSetNvmapFD& params) {
    LOG_DEBUG(Service_NVDRV, "called, fd={}", params.nvmap_fd);

    nvmap_fd = params.nvmap_fd;
    return NvResult::Success;
}

NvResult nvhost_gpu::SetClientData(IoctlClientData& params) {
    LOG_DEBUG(Service_NVDRV, "called");
    user_data = params.data;
    return NvResult::Success;
}

NvResult nvhost_gpu::GetClientData(IoctlClientData& params) {
    LOG_DEBUG(Service_NVDRV, "called");
    params.data = user_data;
    return NvResult::Success;
}

NvResult nvhost_gpu::ZCullBind(IoctlZCullBind& params) {
    zcull_params = params;
    LOG_DEBUG(Service_NVDRV, "called, gpu_va={:X}, mode={:X}", zcull_params.gpu_va,
              zcull_params.mode);
    return NvResult::Success;
}

NvResult nvhost_gpu::SetErrorNotifier(IoctlSetErrorNotifier& params) {
    LOG_DEBUG(Service_NVDRV, "called, offset={:#X}, size={:#X}, mem={:#X}",
              params.offset, params.size, params.mem);

    if (!params.mem || !params.size) {
        std::scoped_lock lk(channel_mutex);
        if (!channel_state->initialized) {
            LOG_CRITICAL(Service_NVDRV, "No address space bound for setting up error notifier!");
            return NvResult::NotInitialized;
        }

        error_notifier_params = {};
        LOG_DEBUG(Service_NVDRV, "Error notifier disabled!");
        return NvResult::Success;
    }

    constexpr u64 error_notification_size = sizeof(IoctlGetErrorNotification);
    if (params.size < error_notification_size) {
        LOG_ERROR(Service_NVDRV, "Error notification size: {:#X} is too small. Need at least {:#X}", params.size,
                  error_notification_size);
        return NvResult::InvalidSize;
    }

    auto handle = nvmap.GetHandle(static_cast<NvCore::NvMap::Handle::Id>(params.mem));
    if (!handle) {
        LOG_ERROR(Service_NVDRV, "Unknown nvmap handle id {:#X}", params.mem);
        return NvResult::BadParameter;
    }

    if (params.offset > handle->size || params.size > (handle->size - params.offset)) {
        LOG_ERROR(Service_NVDRV, "Error notifier out of bounds: offset={:#X} size={:#X} handle size={:#X}", params.offset,
                  params.size, handle->size);
        return NvResult::InvalidSize;
    }

    u64 write_address, write_offset, handle_id;
    {
        std::scoped_lock lk(channel_mutex);
        if (!channel_state->initialized) {
            LOG_CRITICAL(Service_NVDRV, "No address space bound for setting up error notifier!");
            return NvResult::NotInitialized;
        }

        error_notifier_params = params;
        write_address = handle->address;
        write_offset = params.offset;
        handle_id = handle->id;
    }

    if (write_address) {
        IoctlGetErrorNotification error_notification{};
        error_notification.status = static_cast<u16>(NotifierStatus::NoError);
        system.ApplicationMemory().WriteBlock(write_address + write_offset, &error_notification, sizeof(error_notification));
    } else {
        LOG_WARNING(Service_NVDRV,
                    "nvmap handle id {:#X} has no virtual address assigned; "
                    "skipping initialization write for error notification!",
                    handle_id);
    }

    return NvResult::Success;
}

void nvhost_gpu::PostErrorNotification(u32 info32, u16 info16, NotifierStatus status) {
    IoctlSetErrorNotifier error_notifier_params_snapshot{};
    Kernel::KEvent *error_notifier_event_snapshot{};
    {
        std::scoped_lock lk(channel_mutex);
        error_notifier_params_snapshot = error_notifier_params;
        error_notifier_event_snapshot = error_notifier_event;
    }

    if (!error_notifier_params_snapshot.mem || error_notifier_params_snapshot.size < sizeof(IoctlGetErrorNotification)) {
        LOG_DEBUG(Service_NVDRV, "PostErrorNotification: notifier not configured or too small!");
        return;
    }

    auto handle = nvmap.GetHandle(static_cast<NvCore::NvMap::Handle::Id>(error_notifier_params_snapshot.mem));
    if (!handle || !handle->address) {
        LOG_ERROR(Service_NVDRV, "PostErrorNotification: invalid handle or virtual address!");
        return;
    }

    IoctlGetErrorNotification error_init{};
    error_init.info32 = info32;
    error_init.info16 = info16;
    error_init.status = static_cast<u16>(status);
    const u64 write_size = std::min<u64>(sizeof(IoctlGetErrorNotification),
                                         error_notifier_params_snapshot.size);
    if (error_notifier_params_snapshot.offset >= handle->size ||
        write_size > (handle->size - error_notifier_params_snapshot.offset)) {
        LOG_ERROR(Service_NVDRV, "PostErrorNotification: bounds check failed!");
        return;
    }

    const u64 virtual_address = handle->address + error_notifier_params_snapshot.offset;
    if (virtual_address < handle->address) {
        LOG_ERROR(Service_NVDRV, "PostErrorNotification: virtual address overflow!");
        return;
    }

    auto &application_memory = system.ApplicationMemory();
    application_memory.WriteBlock(virtual_address, &error_init, write_size);

    if (error_notifier_event_snapshot) {
        error_notifier_event_snapshot->Signal();
    }
}

NvResult nvhost_gpu::SetChannelPriority(IoctlChannelSetPriority& params) {
    channel_priority = params.priority;
    LOG_INFO(Service_NVDRV, "called, priority={:X}", channel_priority);

    switch (static_cast<ChannelPriority>(channel_priority)) {
    case ChannelPriority::Low: channel_timeslice = 1300; break;
    case ChannelPriority::Medium: channel_timeslice = 2600; break;
    case ChannelPriority::High: channel_timeslice = 5200; break;
    default : return NvResult::BadParameter;
    }

    return NvResult::Success;
}

NvResult nvhost_gpu::AllocGPFIFOEx(IoctlAllocGpfifoEx& params, DeviceFD fd) {
    LOG_DEBUG(Service_NVDRV, "called, num_entries={:X}, flags={:X}, reserved1={:X}, "
              "reserved2={:X}, reserved3={:X}",
              params.num_entries, params.flags, params.reserved[0], params.reserved[1],
              params.reserved[2]);

    if (channel_state->initialized) {
        LOG_DEBUG(Service_NVDRV, "Channel already initialized; AllocGPFIFOEx returning AlreadyAllocated");
        return NvResult::AlreadyAllocated;
    }

    u64 program_id{};
    if (auto* const session = core.GetSession(sessions[fd]); session != nullptr) {
        program_id = session->process->GetProgramId();
    }

    // Store program id for later lazy initialization
    channel_state->program_id = program_id;

    // If address space is not yet bound, defer channel initialization.
    if (!channel_state->memory_manager) {
        params.fence_out = syncpoint_manager.GetSyncpointFence(channel_syncpoint);
        return NvResult::Success;
    }

    system.GPU().InitChannel(*channel_state, program_id);

    params.fence_out = syncpoint_manager.GetSyncpointFence(channel_syncpoint);

    return NvResult::Success;
}

NvResult nvhost_gpu::AllocGPFIFOEx2(IoctlAllocGpfifoEx& params, DeviceFD fd) {
    LOG_DEBUG(Service_NVDRV,
              "called, num_entries={:X}, flags={:X}, reserved1={:X}, "
              "reserved2={:X}, reserved3={:X}",
              params.num_entries, params.flags, params.reserved[0], params.reserved[1],
              params.reserved[2]);

    if (channel_state->initialized) {
        LOG_DEBUG(Service_NVDRV, "Channel already initialized; AllocGPFIFOEx2 returning AlreadyAllocated");
        return NvResult::AlreadyAllocated;
    }

    u64 program_id{};
    if (auto* const session = core.GetSession(sessions[fd]); session != nullptr) {
        program_id = session->process->GetProgramId();
    }

    // Store program id for later lazy initialization
    channel_state->program_id = program_id;

    // If address space is not yet bound, defer channel initialization.
    if (!channel_state->memory_manager) {
        params.fence_out = syncpoint_manager.GetSyncpointFence(channel_syncpoint);
        return NvResult::Success;
    }

    system.GPU().InitChannel(*channel_state, program_id);

    params.fence_out = syncpoint_manager.GetSyncpointFence(channel_syncpoint);

    return NvResult::Success;
}

s32_le nvhost_gpu::GetObjectContextClassNumberIndex(CtxClasses class_number) {
    constexpr s32_le invalid_class_number_index = -1;
    switch (class_number) {
    case CtxClasses::Ctx2D: return 0;
    case CtxClasses::Ctx3D: return 1;
    case CtxClasses::CtxCompute: return 2;
    case CtxClasses::CtxKepler: return 3;
    case CtxClasses::CtxDMA: return 4;
    case CtxClasses::CtxChannelGPFIFO: return 5;
    default: return invalid_class_number_index;
    }
}

NvResult nvhost_gpu::AllocateObjectContext(IoctlAllocObjCtx& params) {
    LOG_DEBUG(Service_NVDRV, "called, class_num={:#X}, flags={:#X}, obj_id={:#X}", params.class_num,
              params.flags, params.obj_id);

    // Do not require channel initialization here: some clients allocate contexts before binding.
    if (!channel_state) {
        LOG_ERROR(Service_NVDRV, "No channel state available!");
        return NvResult::InvalidState;
    }

    std::scoped_lock lk(channel_mutex);

    if (params.flags) {
        LOG_WARNING(Service_NVDRV, "non-zero flags={:#X} for class={:#X}", params.flags,
                    params.class_num);

        constexpr u32 allowed_mask{};
        params.flags = allowed_mask;
    }

    s32_le ctx_class_number_index =
        GetObjectContextClassNumberIndex(static_cast<CtxClasses>(params.class_num));
    if (ctx_class_number_index < 0) {
        LOG_ERROR(Service_NVDRV, "Invalid class number for object context: {:#X}",
                  params.class_num);
        return NvResult::BadParameter;
    }

    if (ctxObjs[ctx_class_number_index].has_value()) {
        LOG_WARNING(Service_NVDRV, "Object context for class {:#X} already allocated on this channel",
                    params.class_num);
        return NvResult::AlreadyAllocated;
    }

    // Defer actual hardware context binding until channel is initialized.
    ctxObjs[ctx_class_number_index] = params;

    return NvResult::Success;
}

static boost::container::small_vector<Tegra::CommandHeader, 512> BuildWaitCommandList(
    NvFence fence) {
    return {
        Tegra::BuildCommandHeader(Tegra::BufferMethods::SyncpointPayload, 1,
                                  Tegra::SubmissionMode::Increasing),
        {fence.value},
        Tegra::BuildCommandHeader(Tegra::BufferMethods::SyncpointOperation, 1,
                                  Tegra::SubmissionMode::Increasing),
        BuildFenceAction(Tegra::Engines::Puller::FenceOperation::Acquire, fence.id),
    };
}

static boost::container::small_vector<Tegra::CommandHeader, 512> BuildIncrementCommandList(
    NvFence fence) {
    boost::container::small_vector<Tegra::CommandHeader, 512> result{
        Tegra::BuildCommandHeader(Tegra::BufferMethods::SyncpointPayload, 1,
                                  Tegra::SubmissionMode::Increasing),
        {}};

    for (u32 count = 0; count < 2; ++count) {
        result.push_back(Tegra::BuildCommandHeader(Tegra::BufferMethods::SyncpointOperation, 1,
                                                   Tegra::SubmissionMode::Increasing));
        result.push_back(
            BuildFenceAction(Tegra::Engines::Puller::FenceOperation::Increment, fence.id));
    }

    return result;
}

static boost::container::small_vector<Tegra::CommandHeader, 512> BuildIncrementWithWfiCommandList(
    NvFence fence) {
    boost::container::small_vector<Tegra::CommandHeader, 512> result{
        Tegra::BuildCommandHeader(Tegra::BufferMethods::WaitForIdle, 1,
                                  Tegra::SubmissionMode::Increasing),
        {}};
    auto increment_list{BuildIncrementCommandList(fence)};
    result.insert(result.end(), increment_list.begin(), increment_list.end());
    return result;
}

NvResult nvhost_gpu::SubmitGPFIFOImpl(IoctlSubmitGpfifo& params, Tegra::CommandList&& entries) {
    LOG_TRACE(Service_NVDRV, "called, gpfifo={:X}, num_entries={:X}, flags={:X}", params.address,
              params.num_entries, params.flags.raw);

    auto& gpu = system.GPU();

    std::scoped_lock lock(channel_mutex);

    // Lazily initialize channel when address space is available
    if (!channel_state->initialized && channel_state->memory_manager) {
        system.GPU().InitChannel(*channel_state, channel_state->program_id);
    }

    const auto bind_id = channel_state->bind_id;

    auto& flags = params.flags;

    if (flags.fence_wait.Value()) {
        if (flags.increment_value.Value()) {
            PostErrorNotification(flags.raw, 0, NotifierStatus::GenericError);
            return NvResult::BadParameter;
        }

        if (!syncpoint_manager.IsFenceSignalled(params.fence)) {
            gpu.PushGPUEntries(bind_id, Tegra::CommandList{BuildWaitCommandList(params.fence)});
        }
    }

    params.fence.id = channel_syncpoint;

    u32 increment{(flags.fence_increment.Value() != 0 ? 2 : 0) +
                  (flags.increment_value.Value() != 0 ? params.fence.value : 0)};
    params.fence.value = syncpoint_manager.IncrementSyncpointMaxExt(channel_syncpoint, increment);
    gpu.PushGPUEntries(bind_id, std::move(entries));

    if (flags.fence_increment.Value()) {
        if (flags.suppress_wfi.Value()) {
            gpu.PushGPUEntries(bind_id,
                               Tegra::CommandList{BuildIncrementCommandList(params.fence)});
        } else {
            gpu.PushGPUEntries(bind_id,
                               Tegra::CommandList{BuildIncrementWithWfiCommandList(params.fence)});
        }
    }

    flags.raw = 0;

    return NvResult::Success;
}

NvResult nvhost_gpu::SubmitGPFIFOBase1(IoctlSubmitGpfifo& params,
                                       std::span<Tegra::CommandListHeader> commands, bool kickoff) {
    if (params.num_entries > commands.size()) {
        LOG_ERROR(Service_NVDRV,
                  "SubmitGPFIFO: num_entries={:#X} > provided commands={:#X}",
                  params.num_entries, commands.size());

        PostErrorNotification(params.num_entries, 0, NotifierStatus::BadGpfifo);
        return NvResult::InvalidSize;
    }

    Tegra::CommandList entries(params.num_entries);
    if (kickoff) {
        system.ApplicationMemory().ReadBlock(params.address, entries.command_lists.data(),
                                             params.num_entries * sizeof(Tegra::CommandListHeader));
    } else {
        std::memcpy(entries.command_lists.data(), commands.data(),
                    params.num_entries * sizeof(Tegra::CommandListHeader));
    }

    return SubmitGPFIFOImpl(params, std::move(entries));
}

NvResult nvhost_gpu::SubmitGPFIFOBase2(IoctlSubmitGpfifo& params,
                                       std::span<const Tegra::CommandListHeader> commands) {
    if (params.num_entries > commands.size()) {
        PostErrorNotification(params.num_entries, 0, NotifierStatus::BadGpfifo);
        return NvResult::InvalidSize;
    }

    Tegra::CommandList entries(params.num_entries);
    std::memcpy(entries.command_lists.data(), commands.data(),
                params.num_entries * sizeof(Tegra::CommandListHeader));
    return SubmitGPFIFOImpl(params, std::move(entries));
}

NvResult nvhost_gpu::GetWaitbase(IoctlGetWaitbase& params) {
    LOG_INFO(Service_NVDRV, "called, unknown={:#X}", params.unknown);

    params.value = 0; // Seems to be hard coded at 0
    return NvResult::Success;
}

NvResult nvhost_gpu::ChannelSetTimeout(IoctlChannelSetTimeout& params) {
    LOG_INFO(Service_NVDRV, "called, timeout={:#X}", params.timeout);

    return NvResult::Success;
}

NvResult nvhost_gpu::ChannelSetTimeslice(IoctlSetTimeslice& params) {
    LOG_INFO(Service_NVDRV, "called, timeslice={:#X}", params.timeslice);

    if (params.timeslice < 1000 || params.timeslice > 5000) {
        return NvResult::BadParameter;
    }

    channel_timeslice = params.timeslice;

    return NvResult::Success;
}

Kernel::KEvent* nvhost_gpu::QueryEvent(u32 event_id) {
    switch (event_id) {
    case 1:
        return sm_exception_breakpoint_int_report_event;
    case 2:
        return sm_exception_breakpoint_pause_report_event;
    case 3:
        return error_notifier_event;
    default:
        LOG_CRITICAL(Service_NVDRV, "Unknown Ctrl GPU Event {}", event_id);
        return nullptr;
    }
}

} // namespace Service::Nvidia::Devices
