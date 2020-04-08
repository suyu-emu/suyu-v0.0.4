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
constexpr u64 Base = 0x80000000ULL;
constexpr u64 Size = 0x100000000ULL;
constexpr u64 End = Base + Size;
constexpr u64 KernelReserveBase = Base + 0x60000;
constexpr u64 SlabHeapBase = KernelReserveBase + 0x85000;
constexpr u64 SlapHeapSize = 0xa21000;
constexpr u64 SlabHeapEnd = SlabHeapBase + SlapHeapSize;
}; // namespace DramMemoryMap

class DeviceMemory : NonCopyable {
public:
    explicit DeviceMemory(Core::System& system);
    ~DeviceMemory();

    template <typename T>
    PAddr GetPhysicalAddr(T* ptr) {
        const auto ptr_addr{reinterpret_cast<uintptr_t>(ptr)};
        const auto base_addr{reinterpret_cast<uintptr_t>(buffer.data())};
        ASSERT(ptr_addr >= base_addr);
        return ptr_addr - base_addr + DramMemoryMap::Base;
    }

    PAddr GetPhysicalAddr(VAddr addr);

    constexpr u8* GetPointer(PAddr addr) {
        return buffer.data() + (addr - DramMemoryMap::Base);
    }

private:
    Common::VirtualBuffer<u8> buffer;
    Core::System& system;
};

} // namespace Core
