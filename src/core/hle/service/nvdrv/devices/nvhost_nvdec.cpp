// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "audio_core/audio_core.h"
#include "common/assert.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/service/nvdrv/core/container.h"
#include "core/hle/service/nvdrv/devices/ioctl_serialization.h"
#include "core/hle/service/nvdrv/devices/nvhost_nvdec.h"
#include "video_core/host1x/host1x.h"
#include "video_core/renderer_base.h"

namespace Service::Nvidia::Devices {

nvhost_nvdec::nvhost_nvdec(Core::System& system_, NvCore::Container& core_)
    : nvhost_nvdec_common{system_, core_, NvCore::ChannelType::NvDec} {}
nvhost_nvdec::~nvhost_nvdec() = default;

NvResult nvhost_nvdec::Ioctl1(DeviceFD fd, Ioctl command, std::span<const u8> input,
                              std::span<u8> output) {
    switch (command.group) {
    case 0x0:
        switch (command.cmd) {
        case 0x1:
            return WrapFixedVariable(this, &nvhost_nvdec::Submit, input, output, fd);
        case 0x2:
            return WrapFixed(this, &nvhost_nvdec::GetSyncpoint, input, output);
        case 0x3:
            return WrapFixed(this, &nvhost_nvdec::GetWaitbase, input, output);
        case 0x7:
            return WrapFixed(this, &nvhost_nvdec::SetSubmitTimeout, input, output);
        case 0x9:
            return WrapFixedVariable(this, &nvhost_nvdec::MapBuffer, input, output, fd);
        case 0xa:
            return WrapFixedVariable(this, &nvhost_nvdec::UnmapBuffer, input, output);
        default:
            break;
        }
        break;
    case 'H':
        switch (command.cmd) {
        case 0x1:
            return WrapFixed(this, &nvhost_nvdec::SetNVMAPfd, input, output);
        default:
            break;
        }
        break;
    }

    UNIMPLEMENTED_MSG("Unimplemented ioctl={:08X}", command.raw);
    return NvResult::NotImplemented;
}

NvResult nvhost_nvdec::Ioctl2(DeviceFD fd, Ioctl command, std::span<const u8> input,
                              std::span<const u8> inline_input, std::span<u8> output) {
    UNIMPLEMENTED_MSG("Unimplemented ioctl={:08X}", command.raw);
    return NvResult::NotImplemented;
}

NvResult nvhost_nvdec::Ioctl3(DeviceFD fd, Ioctl command, std::span<const u8> input,
                              std::span<u8> output, std::span<u8> inline_output) {
    UNIMPLEMENTED_MSG("Unimplemented ioctl={:08X}", command.raw);
    return NvResult::NotImplemented;
}

void nvhost_nvdec::OnOpen(NvCore::SessionId session_id, DeviceFD fd) {
    LOG_INFO(Service_NVDRV, "NVDEC video stream started");
    system.SetNVDECActive(true);
    sessions[fd] = session_id;
    host1x.StartDevice(fd, Tegra::Host1x::ChannelType::NvDec, channel_syncpoint);
}

void nvhost_nvdec::OnClose(DeviceFD fd) {
    LOG_INFO(Service_NVDRV, "NVDEC video stream ended");
    host1x.StopDevice(fd, Tegra::Host1x::ChannelType::NvDec);
    system.SetNVDECActive(false);
    auto it = sessions.find(fd);
    if (it != sessions.end()) {
        sessions.erase(it);
    }
}

} // namespace Service::Nvidia::Devices
