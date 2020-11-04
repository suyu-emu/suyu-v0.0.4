// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include "common/assert.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/service/nvdrv/devices/nvhost_gpu.h"
#include "core/hle/service/nvdrv/syncpoint_manager.h"
#include "core/memory.h"
#include "video_core/gpu.h"
#include "video_core/memory_manager.h"

namespace Service::Nvidia::Devices {

nvhost_gpu::nvhost_gpu(Core::System& system, std::shared_ptr<nvmap> nvmap_dev,
                       SyncpointManager& syncpoint_manager)
    : nvdevice(system), nvmap_dev(std::move(nvmap_dev)), syncpoint_manager{syncpoint_manager} {
    channel_fence.id = syncpoint_manager.AllocateSyncpoint();
    channel_fence.value = system.GPU().GetSyncpointValue(channel_fence.id);
}

nvhost_gpu::~nvhost_gpu() = default;

u32 nvhost_gpu::ioctl(Ioctl command, const std::vector<u8>& input, const std::vector<u8>& input2,
                      std::vector<u8>& output, std::vector<u8>& output2, IoctlCtrl& ctrl,
                      IoctlVersion version) {
    LOG_DEBUG(Service_NVDRV, "called, command=0x{:08X}, input_size=0x{:X}, output_size=0x{:X}",
              command.raw, input.size(), output.size());

    switch (static_cast<IoctlCommand>(command.raw)) {
    case IoctlCommand::IocSetNVMAPfdCommand:
        return SetNVMAPfd(input, output);
    case IoctlCommand::IocSetClientDataCommand:
        return SetClientData(input, output);
    case IoctlCommand::IocGetClientDataCommand:
        return GetClientData(input, output);
    case IoctlCommand::IocZCullBind:
        return ZCullBind(input, output);
    case IoctlCommand::IocSetErrorNotifierCommand:
        return SetErrorNotifier(input, output);
    case IoctlCommand::IocChannelSetPriorityCommand:
        return SetChannelPriority(input, output);
    case IoctlCommand::IocAllocGPFIFOEx2Command:
        return AllocGPFIFOEx2(input, output);
    case IoctlCommand::IocAllocObjCtxCommand:
        return AllocateObjectContext(input, output);
    case IoctlCommand::IocChannelGetWaitbaseCommand:
        return GetWaitbase(input, output);
    case IoctlCommand::IocChannelSetTimeoutCommand:
        return ChannelSetTimeout(input, output);
    case IoctlCommand::IocChannelSetTimeslice:
        return ChannelSetTimeslice(input, output);
    default:
        break;
    }

    if (command.group == NVGPU_IOCTL_MAGIC) {
        if (command.cmd == NVGPU_IOCTL_CHANNEL_SUBMIT_GPFIFO) {
            return SubmitGPFIFO(input, output);
        }
        if (command.cmd == NVGPU_IOCTL_CHANNEL_KICKOFF_PB) {
            return KickoffPB(input, output, input2, version);
        }
    }

    UNIMPLEMENTED_MSG("Unimplemented ioctl");
    return 0;
};

u32 nvhost_gpu::SetNVMAPfd(const std::vector<u8>& input, std::vector<u8>& output) {
    IoctlSetNvmapFD params{};
    std::memcpy(&params, input.data(), input.size());
    LOG_DEBUG(Service_NVDRV, "called, fd={}", params.nvmap_fd);

    nvmap_fd = params.nvmap_fd;
    return 0;
}

u32 nvhost_gpu::SetClientData(const std::vector<u8>& input, std::vector<u8>& output) {
    LOG_DEBUG(Service_NVDRV, "called");

    IoctlClientData params{};
    std::memcpy(&params, input.data(), input.size());
    user_data = params.data;
    return 0;
}

u32 nvhost_gpu::GetClientData(const std::vector<u8>& input, std::vector<u8>& output) {
    LOG_DEBUG(Service_NVDRV, "called");

    IoctlClientData params{};
    std::memcpy(&params, input.data(), input.size());
    params.data = user_data;
    std::memcpy(output.data(), &params, output.size());
    return 0;
}

u32 nvhost_gpu::ZCullBind(const std::vector<u8>& input, std::vector<u8>& output) {
    std::memcpy(&zcull_params, input.data(), input.size());
    LOG_DEBUG(Service_NVDRV, "called, gpu_va={:X}, mode={:X}", zcull_params.gpu_va,
              zcull_params.mode);

    std::memcpy(output.data(), &zcull_params, output.size());
    return 0;
}

u32 nvhost_gpu::SetErrorNotifier(const std::vector<u8>& input, std::vector<u8>& output) {
    IoctlSetErrorNotifier params{};
    std::memcpy(&params, input.data(), input.size());
    LOG_WARNING(Service_NVDRV, "(STUBBED) called, offset={:X}, size={:X}, mem={:X}", params.offset,
                params.size, params.mem);

    std::memcpy(output.data(), &params, output.size());
    return 0;
}

u32 nvhost_gpu::SetChannelPriority(const std::vector<u8>& input, std::vector<u8>& output) {
    std::memcpy(&channel_priority, input.data(), input.size());
    LOG_DEBUG(Service_NVDRV, "(STUBBED) called, priority={:X}", channel_priority);

    return 0;
}

u32 nvhost_gpu::AllocGPFIFOEx2(const std::vector<u8>& input, std::vector<u8>& output) {
    IoctlAllocGpfifoEx2 params{};
    std::memcpy(&params, input.data(), input.size());
    LOG_WARNING(Service_NVDRV,
                "(STUBBED) called, num_entries={:X}, flags={:X}, unk0={:X}, "
                "unk1={:X}, unk2={:X}, unk3={:X}",
                params.num_entries, params.flags, params.unk0, params.unk1, params.unk2,
                params.unk3);

    channel_fence.value = system.GPU().GetSyncpointValue(channel_fence.id);

    params.fence_out = channel_fence;

    std::memcpy(output.data(), &params, output.size());
    return 0;
}

u32 nvhost_gpu::AllocateObjectContext(const std::vector<u8>& input, std::vector<u8>& output) {
    IoctlAllocObjCtx params{};
    std::memcpy(&params, input.data(), input.size());
    LOG_WARNING(Service_NVDRV, "(STUBBED) called, class_num={:X}, flags={:X}", params.class_num,
                params.flags);

    params.obj_id = 0x0;
    std::memcpy(output.data(), &params, output.size());
    return 0;
}

static std::vector<Tegra::CommandHeader> BuildWaitCommandList(Fence fence) {
    return {
        Tegra::BuildCommandHeader(Tegra::BufferMethods::FenceValue, 1,
                                  Tegra::SubmissionMode::Increasing),
        {fence.value},
        Tegra::BuildCommandHeader(Tegra::BufferMethods::FenceAction, 1,
                                  Tegra::SubmissionMode::Increasing),
        Tegra::GPU::FenceAction::Build(Tegra::GPU::FenceOperation::Acquire, fence.id),
    };
}

static std::vector<Tegra::CommandHeader> BuildIncrementCommandList(Fence fence, u32 add_increment) {
    std::vector<Tegra::CommandHeader> result{
        Tegra::BuildCommandHeader(Tegra::BufferMethods::FenceValue, 1,
                                  Tegra::SubmissionMode::Increasing),
        {}};

    for (u32 count = 0; count < add_increment; ++count) {
        result.emplace_back(Tegra::BuildCommandHeader(Tegra::BufferMethods::FenceAction, 1,
                                                      Tegra::SubmissionMode::Increasing));
        result.emplace_back(
            Tegra::GPU::FenceAction::Build(Tegra::GPU::FenceOperation::Increment, fence.id));
    }

    return result;
}

static std::vector<Tegra::CommandHeader> BuildIncrementWithWfiCommandList(Fence fence,
                                                                          u32 add_increment) {
    std::vector<Tegra::CommandHeader> result{
        Tegra::BuildCommandHeader(Tegra::BufferMethods::WaitForInterrupt, 1,
                                  Tegra::SubmissionMode::Increasing),
        {}};
    const std::vector<Tegra::CommandHeader> increment{
        BuildIncrementCommandList(fence, add_increment)};

    result.insert(result.end(), increment.begin(), increment.end());

    return result;
}

u32 nvhost_gpu::SubmitGPFIFOImpl(IoctlSubmitGpfifo& params, std::vector<u8>& output,
                                 Tegra::CommandList&& entries) {
    LOG_TRACE(Service_NVDRV, "called, gpfifo={:X}, num_entries={:X}, flags={:X}", params.address,
              params.num_entries, params.flags.raw);

    auto& gpu = system.GPU();

    params.fence_out.id = channel_fence.id;

    if (params.flags.add_wait.Value() &&
        !syncpoint_manager.IsSyncpointExpired(params.fence_out.id, params.fence_out.value)) {
        gpu.PushGPUEntries(Tegra::CommandList{BuildWaitCommandList(params.fence_out)});
    }

    if (params.flags.add_increment.Value() || params.flags.increment.Value()) {
        const u32 increment_value = params.flags.increment.Value() ? params.fence_out.value : 0;
        params.fence_out.value = syncpoint_manager.IncreaseSyncpoint(
            params.fence_out.id, params.AddIncrementValue() + increment_value);
    } else {
        params.fence_out.value = syncpoint_manager.GetSyncpointMax(params.fence_out.id);
    }

    entries.RefreshIntegrityChecks(gpu);
    gpu.PushGPUEntries(std::move(entries));

    if (params.flags.add_increment.Value()) {
        if (params.flags.suppress_wfi) {
            gpu.PushGPUEntries(Tegra::CommandList{
                BuildIncrementCommandList(params.fence_out, params.AddIncrementValue())});
        } else {
            gpu.PushGPUEntries(Tegra::CommandList{
                BuildIncrementWithWfiCommandList(params.fence_out, params.AddIncrementValue())});
        }
    }

    std::memcpy(output.data(), &params, sizeof(IoctlSubmitGpfifo));
    return 0;
}

u32 nvhost_gpu::SubmitGPFIFO(const std::vector<u8>& input, std::vector<u8>& output) {
    if (input.size() < sizeof(IoctlSubmitGpfifo)) {
        UNIMPLEMENTED();
    }
    IoctlSubmitGpfifo params{};
    std::memcpy(&params, input.data(), sizeof(IoctlSubmitGpfifo));

    Tegra::CommandList entries(params.num_entries);
    std::memcpy(entries.command_lists.data(), &input[sizeof(IoctlSubmitGpfifo)],
                params.num_entries * sizeof(Tegra::CommandListHeader));

    return SubmitGPFIFOImpl(params, output, std::move(entries));
}

u32 nvhost_gpu::KickoffPB(const std::vector<u8>& input, std::vector<u8>& output,
                          const std::vector<u8>& input2, IoctlVersion version) {
    if (input.size() < sizeof(IoctlSubmitGpfifo)) {
        UNIMPLEMENTED();
    }
    IoctlSubmitGpfifo params{};
    std::memcpy(&params, input.data(), sizeof(IoctlSubmitGpfifo));

    Tegra::CommandList entries(params.num_entries);
    if (version == IoctlVersion::Version2) {
        std::memcpy(entries.command_lists.data(), input2.data(),
                    params.num_entries * sizeof(Tegra::CommandListHeader));
    } else {
        system.Memory().ReadBlock(params.address, entries.command_lists.data(),
                                  params.num_entries * sizeof(Tegra::CommandListHeader));
    }

    return SubmitGPFIFOImpl(params, output, std::move(entries));
}

u32 nvhost_gpu::GetWaitbase(const std::vector<u8>& input, std::vector<u8>& output) {
    IoctlGetWaitbase params{};
    std::memcpy(&params, input.data(), sizeof(IoctlGetWaitbase));
    LOG_INFO(Service_NVDRV, "called, unknown=0x{:X}", params.unknown);

    params.value = 0; // Seems to be hard coded at 0
    std::memcpy(output.data(), &params, output.size());
    return 0;
}

u32 nvhost_gpu::ChannelSetTimeout(const std::vector<u8>& input, std::vector<u8>& output) {
    IoctlChannelSetTimeout params{};
    std::memcpy(&params, input.data(), sizeof(IoctlChannelSetTimeout));
    LOG_INFO(Service_NVDRV, "called, timeout=0x{:X}", params.timeout);

    return 0;
}

u32 nvhost_gpu::ChannelSetTimeslice(const std::vector<u8>& input, std::vector<u8>& output) {
    IoctlSetTimeslice params{};
    std::memcpy(&params, input.data(), sizeof(IoctlSetTimeslice));
    LOG_INFO(Service_NVDRV, "called, timeslice=0x{:X}", params.timeslice);

    channel_timeslice = params.timeslice;

    return 0;
}

} // namespace Service::Nvidia::Devices
