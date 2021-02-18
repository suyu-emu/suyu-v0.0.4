// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/service/nvdrv/devices/nvhost_nvdec.h"
#include "video_core/memory_manager.h"
#include "video_core/renderer_base.h"

namespace Service::Nvidia::Devices {

nvhost_nvdec::nvhost_nvdec(Core::System& system, std::shared_ptr<nvmap> nvmap_dev,
                           SyncpointManager& syncpoint_manager)
    : nvhost_nvdec_common(system, std::move(nvmap_dev), syncpoint_manager) {}
nvhost_nvdec::~nvhost_nvdec() = default;

NvResult nvhost_nvdec::Ioctl1(Ioctl command, const std::vector<u8>& input,
                              std::vector<u8>& output) {
    switch (command.group) {
    case 0x0:
        switch (command.cmd) {
        case 0x1:
            return Submit(input, output);
        case 0x2:
            return GetSyncpoint(input, output);
        case 0x3:
            return GetWaitbase(input, output);
        case 0x7:
            return SetSubmitTimeout(input, output);
        case 0x9:
            return MapBuffer(input, output);
        case 0xa: {
            if (command.length == 0x1c) {
                LOG_INFO(Service_NVDRV, "NVDEC video stream ended");
                Tegra::ChCommandHeaderList cmdlist{{0xDEADB33F}};
                system.GPU().PushCommandBuffer(cmdlist);
            }
            return UnmapBuffer(input, output);
        }
        default:
            break;
        }
        break;
    case 'H':
        switch (command.cmd) {
        case 0x1:
            return SetNVMAPfd(input);
        default:
            break;
        }
        break;
    }

    UNIMPLEMENTED_MSG("Unimplemented ioctl={:08X}", command.raw);
    return NvResult::NotImplemented;
}

NvResult nvhost_nvdec::Ioctl2(Ioctl command, const std::vector<u8>& input,
                              const std::vector<u8>& inline_input, std::vector<u8>& output) {
    UNIMPLEMENTED_MSG("Unimplemented ioctl={:08X}", command.raw);
    return NvResult::NotImplemented;
}

NvResult nvhost_nvdec::Ioctl3(Ioctl command, const std::vector<u8>& input, std::vector<u8>& output,
                              std::vector<u8>& inline_output) {
    UNIMPLEMENTED_MSG("Unimplemented ioctl={:08X}", command.raw);
    return NvResult::NotImplemented;
}

} // namespace Service::Nvidia::Devices
