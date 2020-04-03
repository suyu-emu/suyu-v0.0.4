// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#ifdef _WIN32
#include <windows.h>
#endif

#include "common/assert.h"
#include "core/core.h"
#include "core/device_memory.h"
#include "core/memory.h"

namespace Core {

constexpr u64 DramSize{4ULL * 1024 * 1024 * 1024};

DeviceMemory::DeviceMemory(System& system) : system{system} {
#ifdef _WIN32
    base = static_cast<u8*>(
        VirtualAlloc(nullptr,                                    // System selects address
                     DramSize,                                   // Size of allocation
                     MEM_RESERVE | MEM_COMMIT | MEM_WRITE_WATCH, // Allocate reserved pages
                     PAGE_READWRITE));                           // Protection = no access
#else
    physical_memory.resize(DramSize);
    base = physical_memory.data();
#endif
}

DeviceMemory::~DeviceMemory() {
#ifdef _WIN32
    ASSERT(VirtualFree(base, DramSize, MEM_RELEASE));
#endif
}

PAddr DeviceMemory::GetPhysicalAddr(VAddr addr) {
    u8* pointer{system.Memory().GetPointer(addr)};
    ASSERT(pointer);
    const uintptr_t offset{static_cast<uintptr_t>(pointer - GetPointer(DramMemoryMap::Base))};
    return DramMemoryMap::Base + offset;
}

u8* DeviceMemory::GetPointer(PAddr addr) {
    ASSERT(addr >= DramMemoryMap::Base);
    ASSERT(addr < DramMemoryMap::Base + DramSize);
    return base + (addr - DramMemoryMap::Base);
}

} // namespace Core
