// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <unordered_map>
#include <vector>
#include "common/common_types.h"
#include "core/hle/service/service.h"

namespace Service::NVFlinger {
class NVFlinger;
}

namespace Service::Nvidia {

namespace Devices {
class nvdevice;
}

struct IoctlFence {
    u32 id;
    u32 value;
};

static_assert(sizeof(IoctlFence) == 8, "IoctlFence has wrong size");

class Module final {
public:
    Module();
    ~Module();

    /// Returns a pointer to one of the available devices, identified by its name.
    template <typename T>
    std::shared_ptr<T> GetDevice(const std::string& name) {
        auto itr = devices.find(name);
        if (itr == devices.end())
            return nullptr;
        return std::static_pointer_cast<T>(itr->second);
    }

    /// Opens a device node and returns a file descriptor to it.
    u32 Open(const std::string& device_name);
    /// Sends an ioctl command to the specified file descriptor.
    u32 Ioctl(u32 fd, u32 command, const std::vector<u8>& input, std::vector<u8>& output);
    /// Closes a device file descriptor and returns operation success.
    ResultCode Close(u32 fd);

private:
    /// Id to use for the next open file descriptor.
    u32 next_fd = 1;

    /// Mapping of file descriptors to the devices they reference.
    std::unordered_map<u32, std::shared_ptr<Devices::nvdevice>> open_files;

    /// Mapping of device node names to their implementation.
    std::unordered_map<std::string, std::shared_ptr<Devices::nvdevice>> devices;
};

/// Registers all NVDRV services with the specified service manager.
void InstallInterfaces(SM::ServiceManager& service_manager, NVFlinger::NVFlinger& nvflinger);

} // namespace Service::Nvidia
