// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/service/nvdrv/devices/nvhost_vic.h"
#include "video_core/memory_manager.h"
#include "video_core/renderer_base.h"

namespace Service::Nvidia::Devices {
nvhost_vic::nvhost_vic(Core::System& system, std::shared_ptr<nvmap> nvmap_dev)
    : nvhost_nvdec_common(system, std::move(nvmap_dev)) {}

nvhost_vic::~nvhost_vic() = default;

u32 nvhost_vic::ioctl(Ioctl command, const std::vector<u8>& input, const std::vector<u8>& input2,
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
    case IoctlCommand::IocMapBuffer4:
    case IoctlCommand::IocMapBufferEx:
        return MapBuffer(input, output);
    case IoctlCommand::IocUnmapBuffer:
    case IoctlCommand::IocUnmapBuffer2:
    case IoctlCommand::IocUnmapBuffer3:
    case IoctlCommand::IocUnmapBufferEx:
        return UnmapBuffer(input, output);
    }

    UNIMPLEMENTED_MSG("Unimplemented ioctl 0x{:X}", command.raw);
    return 0;
}

} // namespace Service::Nvidia::Devices
