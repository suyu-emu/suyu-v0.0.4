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

nvhost_nvdec::nvhost_nvdec(Core::System& system, std::shared_ptr<nvmap> nvmap_dev)
    : nvhost_nvdec_common(system, std::move(nvmap_dev)) {}
nvhost_nvdec::~nvhost_nvdec() = default;

u32 nvhost_nvdec::ioctl(Ioctl command, const std::vector<u8>& input, const std::vector<u8>& input2,
                        std::vector<u8>& output, std::vector<u8>& output2, IoctlCtrl& ctrl,
                        IoctlVersion version) {
    LOG_DEBUG(Service_NVDRV, "called, command=0x{:08X}, input_size=0x{:X}, output_size=0x{:X}",
              command.raw, input.size(), output.size());

    switch (static_cast<IoctlCommand>(command.raw)) {
    case IoctlCommand::IocSetNVMAPfdCommand:
        return SetNVMAPfd(input);
    case IoctlCommand::IocSubmit:
        return Submit(input, output);
    case IoctlCommand::IocGetSyncpoint:
        return GetSyncpoint(input, output);
    case IoctlCommand::IocGetWaitbase:
        return GetWaitbase(input, output);
    case IoctlCommand::IocMapBuffer:
    case IoctlCommand::IocMapBuffer2:
    case IoctlCommand::IocMapBuffer3:
    case IoctlCommand::IocMapBufferEx:
        return MapBuffer(input, output);
    case IoctlCommand::IocUnmapBufferEx: {
        // This command is sent when the video stream has ended, flush all video contexts
        // This is usually sent in the folowing order: vic, nvdec, vic.
        // Inform the GPU to clear any remaining nvdec buffers when this is detected.
        LOG_INFO(Service_NVDRV, "NVDEC video stream ended");
        Tegra::ChCommandHeaderList cmdlist(1);
        cmdlist[0] = Tegra::ChCommandHeader{0xDEADB33F};
        system.GPU().PushCommandBuffer(cmdlist);
        [[fallthrough]]; // fallthrough to unmap buffers
    };
    case IoctlCommand::IocUnmapBuffer:
    case IoctlCommand::IocUnmapBuffer2:
    case IoctlCommand::IocUnmapBuffer3:
        return UnmapBuffer(input, output);
    case IoctlCommand::IocSetSubmitTimeout:
        return SetSubmitTimeout(input, output);
    }

    UNIMPLEMENTED_MSG("Unimplemented ioctl 0x{:X}", command.raw);
    return 0;
}

} // namespace Service::Nvidia::Devices
