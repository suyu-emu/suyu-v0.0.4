// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/assert.h"
#include "common/common_funcs.h"
#include "common/virtual_buffer.h"

namespace Core {

class System;

namespace DramMemoryMap {
enum : u64 {
    Base = 0x80000000ULL,
    Size = 0x100000000ULL,
    End = Base + Size,
    KernelReserveBase = Base + 0x60000,
    SlabHeapBase = KernelReserveBase + 0x85000,
    SlapHeapSize = 0xa21000,
    SlabHeapEnd = SlabHeapBase + SlapHeapSize,
};
}; // namespace DramMemoryMap

class DeviceMemory : NonCopyable {
public:
    explicit DeviceMemory(Core::System& system);
    ~DeviceMemory();

    template <typename T>
    PAddr GetPhysicalAddr(const T* ptr) const {
        return (reinterpret_cast<uintptr_t>(ptr) - reinterpret_cast<uintptr_t>(buffer.data())) +
               DramMemoryMap::Base;
    }

    u8* GetPointer(PAddr addr) {
        return buffer.data() + (addr - DramMemoryMap::Base);
    }

    const u8* GetPointer(PAddr addr) const {
        return buffer.data() + (addr - DramMemoryMap::Base);
    }

private:
    Common::VirtualBuffer<u8> buffer;
    Core::System& system;
};

} // namespace Core
