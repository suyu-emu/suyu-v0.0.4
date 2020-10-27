// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/nvdrv/devices/nvhost_nvdec_common.h"

namespace Service::Nvidia::Devices {
class nvmap;

class nvhost_vic final : public nvhost_nvdec_common {
public:
    explicit nvhost_vic(Core::System& system, std::shared_ptr<nvmap> nvmap_dev);
    ~nvhost_vic();
    u32 ioctl(Ioctl command, const std::vector<u8>& input, const std::vector<u8>& input2,
              std::vector<u8>& output, std::vector<u8>& output2, IoctlCtrl& ctrl,
              IoctlVersion version) override;

private:
    enum class IoctlCommand : u32_le {
        IocSetNVMAPfdCommand = 0x40044801,
        IocSubmit = 0xC0400001,
        IocGetSyncpoint = 0xC0080002,
        IocGetWaitbase = 0xC0080003,
        IocMapBuffer = 0xC01C0009,
        IocMapBuffer2 = 0xC0340009,
        IocMapBuffer3 = 0xC0140009,
        IocMapBuffer4 = 0xC00C0009,
        IocMapBufferEx = 0xC03C0009,
        IocUnmapBuffer = 0xC03C000A,
        IocUnmapBuffer2 = 0xC034000A,
        IocUnmapBuffer3 = 0xC00C000A,
        IocUnmapBufferEx = 0xC01C000A,
    };
};
} // namespace Service::Nvidia::Devices
