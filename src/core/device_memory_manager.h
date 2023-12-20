// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <deque>
#include <memory>

#include "common/common_types.h"
#include "common/virtual_buffer.h"

namespace Core {

class DeviceMemory;

namespace Memory {
class Memory;
}

template <typename DTraits>
struct DeviceMemoryManagerAllocator;

template <typename Traits>
class DeviceMemoryManager {
    using DeviceInterface = typename Traits::DeviceInterface;

public:
    DeviceMemoryManager(const DeviceMemory& device_memory);
    ~DeviceMemoryManager();

    void BindInterface(DeviceInterface* interface);

    DAddr Allocate(size_t size);
    void AllocateFixed(DAddr start, size_t size);
    DAddr AllocatePinned(size_t size);
    void Free(DAddr start, size_t size);

    void Map(DAddr address, VAddr virtual_address, size_t size, size_t p_id);
    void Unmap(DAddr address, size_t size);

    // Write / Read
    template <typename T>
    T* GetPointer(DAddr address);

    template <typename T>
    const T* GetPointer(DAddr address) const;

    template <typename T>
    void Write(DAddr address, T value);

    template <typename T>
    T Read(DAddr address) const;

    void ReadBlock(DAddr address, void* dest_pointer, size_t size);
    void WriteBlock(DAddr address, void* src_pointer, size_t size);

    size_t RegisterProcess(Memory::Memory* memory);
    void UnregisterProcess(size_t id);

private:
    static constexpr bool supports_pinning = Traits::supports_pinning;
    static constexpr size_t device_virtual_bits = Traits::device_virtual_bits;
    static constexpr size_t device_as_size = 1ULL << device_virtual_bits;
    static constexpr size_t physical_max_bits = 33;
    static constexpr size_t page_bits = 12;
    static constexpr u32 physical_address_base = 1U << page_bits;

    template <typename T>
    T* GetPointerFromRaw(PAddr addr) {
        return reinterpret_cast<T*>(physical_base + addr);
    }

    template <typename T>
    const T* GetPointerFromRaw(PAddr addr) const {
        return reinterpret_cast<T*>(physical_base + addr);
    }

    template <typename T>
    PAddr GetRawPhysicalAddr(const T* ptr) const {
        return static_cast<PAddr>(reinterpret_cast<uintptr_t>(ptr) - physical_base);
    }

    void WalkBlock(const DAddr addr, const std::size_t size, auto on_unmapped, auto on_memory,
                   auto increment);

    std::unique_ptr<DeviceMemoryManagerAllocator<Traits>> impl;

    const uintptr_t physical_base;
    DeviceInterface* interface;
    Common::VirtualBuffer<u32> compressed_physical_ptr;
    Common::VirtualBuffer<u32> compressed_device_addr;

    std::deque<size_t> id_pool;
    std::deque<Memory::Memory*> registered_processes;
};

} // namespace Core