// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/core.h"
#include "core/device_memory.h"
#include "core/memory.h"

namespace Core {

DeviceMemory::DeviceMemory(System& system) : buffer{DramMemoryMap::Size}, system{system} {}

DeviceMemory::~DeviceMemory() = default;

PAddr DeviceMemory::GetPhysicalAddr(VAddr addr) {
    const u8* const base{system.Memory().GetPointer(addr)};
    ASSERT(base);
    const uintptr_t offset{static_cast<uintptr_t>(base - GetPointer(DramMemoryMap::Base))};
    return DramMemoryMap::Base + offset;
}

} // namespace Core
