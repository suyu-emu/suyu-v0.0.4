// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "core/device_memory.h"

namespace Kernel {

constexpr std::size_t KernelAslrAlignment = 2 * 1024 * 1024;
constexpr std::size_t KernelVirtualAddressSpaceWidth = 1ULL << 39;
constexpr std::size_t KernelPhysicalAddressSpaceWidth = 1ULL << 48;
constexpr std::size_t KernelVirtualAddressSpaceBase = 0ULL - KernelVirtualAddressSpaceWidth;
constexpr std::size_t KernelVirtualAddressSpaceEnd =
    KernelVirtualAddressSpaceBase + (KernelVirtualAddressSpaceWidth - KernelAslrAlignment);
constexpr std::size_t KernelVirtualAddressSpaceLast = KernelVirtualAddressSpaceEnd - 1;
constexpr std::size_t KernelVirtualAddressSpaceSize =
    KernelVirtualAddressSpaceEnd - KernelVirtualAddressSpaceBase;

constexpr bool IsKernelAddressKey(VAddr key) {
    return KernelVirtualAddressSpaceBase <= key && key <= KernelVirtualAddressSpaceLast;
}

constexpr bool IsKernelAddress(VAddr address) {
    return KernelVirtualAddressSpaceBase <= address && address < KernelVirtualAddressSpaceEnd;
}

class KMemoryRegion final {
    friend class KMemoryLayout;

public:
    constexpr PAddr StartAddress() const {
        return start_address;
    }

    constexpr PAddr EndAddress() const {
        return end_address;
    }

private:
    constexpr KMemoryRegion() = default;
    constexpr KMemoryRegion(PAddr start_address, PAddr end_address)
        : start_address{start_address}, end_address{end_address} {}

    const PAddr start_address{};
    const PAddr end_address{};
};

class KMemoryLayout final {
public:
    constexpr const KMemoryRegion& Application() const {
        return application;
    }

    constexpr const KMemoryRegion& Applet() const {
        return applet;
    }

    constexpr const KMemoryRegion& System() const {
        return system;
    }

    static constexpr KMemoryLayout GetDefaultLayout() {
        constexpr std::size_t application_size{0xcd500000};
        constexpr std::size_t applet_size{0x1fb00000};
        constexpr PAddr application_start_address{Core::DramMemoryMap::End - application_size};
        constexpr PAddr application_end_address{Core::DramMemoryMap::End};
        constexpr PAddr applet_start_address{application_start_address - applet_size};
        constexpr PAddr applet_end_address{applet_start_address + applet_size};
        constexpr PAddr system_start_address{Core::DramMemoryMap::SlabHeapEnd};
        constexpr PAddr system_end_address{applet_start_address};
        return {application_start_address, application_end_address, applet_start_address,
                applet_end_address,        system_start_address,    system_end_address};
    }

private:
    constexpr KMemoryLayout(PAddr application_start_address, std::size_t application_size,
                            PAddr applet_start_address, std::size_t applet_size,
                            PAddr system_start_address, std::size_t system_size)
        : application{application_start_address, application_size},
          applet{applet_start_address, applet_size}, system{system_start_address, system_size} {}

    const KMemoryRegion application;
    const KMemoryRegion applet;
    const KMemoryRegion system;
};

} // namespace Kernel
