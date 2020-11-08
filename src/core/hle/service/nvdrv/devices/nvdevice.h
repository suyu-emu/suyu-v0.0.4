// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include "common/bit_field.h"
#include "common/common_types.h"
#include "common/swap.h"
#include "core/hle/service/nvdrv/nvdata.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::Nvidia::Devices {

/// Represents an abstract nvidia device node. It is to be subclassed by concrete device nodes to
/// implement the ioctl interface.
class nvdevice {
public:
    explicit nvdevice(Core::System& system) : system{system} {}
    virtual ~nvdevice() = default;

    virtual NvResult Ioctl1(Ioctl command, const std::vector<u8>& input,
                            std::vector<u8>& output) = 0;
    virtual NvResult Ioctl2(Ioctl command, const std::vector<u8>& input,
                            const std::vector<u8>& inline_input, std::vector<u8>& output) = 0;
    virtual NvResult Ioctl3(Ioctl command, const std::vector<u8>& input, std::vector<u8>& output,
                            std::vector<u8>& inline_output) = 0;

protected:
    Core::System& system;
};

} // namespace Service::Nvidia::Devices
