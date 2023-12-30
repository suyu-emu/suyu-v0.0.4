// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <atomic>
#include <deque>
#include <memory>
#include <mutex>

#include "common/common_types.h"
#include "common/scratch_buffer.h"
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
    using DeviceMethods = Traits::DeviceMethods;

public:
    DeviceMemoryManager(const DeviceMemory& device_memory);
    ~DeviceMemoryManager();

    void BindInterface(DeviceInterface* interface);

    DAddr Allocate(size_t size);
    void AllocateFixed(DAddr start, size_t size);
    void Free(DAddr start, size_t size);

    void Map(DAddr address, VAddr virtual_address, size_t size, size_t process_id,
             bool track = false);

    void Unmap(DAddr address, size_t size);

    void TrackContinuityImpl(DAddr address, VAddr virtual_address, size_t size, size_t process_id);
    void TrackContinuity(DAddr address, VAddr virtual_address, size_t size, size_t process_id) {
        std::scoped_lock lk(mapping_guard);
        TrackContinuityImpl(address, virtual_address, size, process_id);
    }

    // Write / Read
    template <typename T>
    T* GetPointer(DAddr address);

    template <typename T>
    const T* GetPointer(DAddr address) const;

    template <typename Func>
    void ApplyOpOnPAddr(PAddr address, Common::ScratchBuffer<u32>& buffer, Func&& operation) {
        DAddr subbits = static_cast<DAddr>(address & page_mask);
        const u32 base = compressed_device_addr[(address >> page_bits)];
        if ((base >> MULTI_FLAG_BITS) == 0) [[likely]] {
            const DAddr d_address = static_cast<DAddr>(base << page_bits) + subbits;
            operation(d_address);
            return;
        }
        InnerGatherDeviceAddresses(buffer, address);
        for (u32 value : buffer) {
            operation(static_cast<DAddr>(value << page_bits) + subbits);
        }
    }

    template <typename Func>
    void ApplyOpOnPointer(const u8* p, Common::ScratchBuffer<u32>& buffer, Func&& operation) {
        PAddr address = GetRawPhysicalAddr<u8>(p);
        ApplyOpOnPAddr(address, buffer, operation);
    }

    PAddr GetPhysicalRawAddressFromDAddr(DAddr address) const {
        PAddr subbits = static_cast<PAddr>(address & page_mask);
        auto paddr = compressed_physical_ptr[(address >> page_bits)];
        if (paddr == 0) {
            return 0;
        }
        return (static_cast<PAddr>(paddr - 1) << page_bits) + subbits;
    }

    template <typename T>
    void Write(DAddr address, T value);

    template <typename T>
    T Read(DAddr address) const;

    u8* GetSpan(const DAddr src_addr, const std::size_t size);
    const u8* GetSpan(const DAddr src_addr, const std::size_t size) const;

    void ReadBlock(DAddr address, void* dest_pointer, size_t size);
    void ReadBlockUnsafe(DAddr address, void* dest_pointer, size_t size);
    void WriteBlock(DAddr address, const void* src_pointer, size_t size);
    void WriteBlockUnsafe(DAddr address, const void* src_pointer, size_t size);

    size_t RegisterProcess(Memory::Memory* memory);
    void UnregisterProcess(size_t id);

    void UpdatePagesCachedCount(DAddr addr, size_t size, s32 delta);

    static constexpr size_t AS_BITS = Traits::device_virtual_bits;

private:
    static constexpr size_t device_virtual_bits = Traits::device_virtual_bits;
    static constexpr size_t device_as_size = 1ULL << device_virtual_bits;
    static constexpr size_t physical_max_bits = 33;
    static constexpr size_t page_bits = 12;
    static constexpr size_t page_size = 1ULL << page_bits;
    static constexpr size_t page_mask = page_size - 1ULL;
    static constexpr u32 physical_address_base = 1U << page_bits;
    static constexpr u32 MULTI_FLAG_BITS = 31;
    static constexpr u32 MULTI_FLAG = 1U << MULTI_FLAG_BITS;
    static constexpr u32 MULTI_MASK = ~MULTI_FLAG;

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

    void InnerGatherDeviceAddresses(Common::ScratchBuffer<u32>& buffer, PAddr address);

    std::unique_ptr<DeviceMemoryManagerAllocator<Traits>> impl;

    const uintptr_t physical_base;
    DeviceInterface* interface;
    Common::VirtualBuffer<u32> compressed_physical_ptr;
    Common::VirtualBuffer<u32> compressed_device_addr;
    Common::VirtualBuffer<u32> continuity_tracker;

    // Process memory interfaces

    std::deque<size_t> id_pool;
    std::deque<Memory::Memory*> registered_processes;

    // Memory protection management

    static constexpr size_t guest_max_as_bits = 39;
    static constexpr size_t guest_as_size = 1ULL << guest_max_as_bits;
    static constexpr size_t guest_mask = guest_as_size - 1ULL;
    static constexpr size_t process_id_start_bit = guest_max_as_bits;

    std::pair<size_t, VAddr> ExtractCPUBacking(size_t page_index) {
        auto content = cpu_backing_address[page_index];
        const VAddr address = content & guest_mask;
        const size_t process_id = static_cast<size_t>(content >> process_id_start_bit);
        return std::make_pair(process_id, address);
    }

    void InsertCPUBacking(size_t page_index, VAddr address, size_t process_id) {
        cpu_backing_address[page_index] = address | (process_id << process_id_start_bit);
    }

    Common::VirtualBuffer<VAddr> cpu_backing_address;
    static constexpr size_t subentries = 8 / sizeof(u8);
    static constexpr size_t subentries_mask = subentries - 1;
    class CounterEntry final {
    public:
        CounterEntry() = default;

        std::atomic_uint8_t& Count(std::size_t page) {
            return values[page & subentries_mask];
        }

        const std::atomic_uint8_t& Count(std::size_t page) const {
            return values[page & subentries_mask];
        }

    private:
        std::array<std::atomic_uint8_t, subentries> values{};
    };
    static_assert(sizeof(CounterEntry) == subentries * sizeof(u8),
                  "CounterEntry should be 8 bytes!");

    static constexpr size_t num_counter_entries =
        (1ULL << (device_virtual_bits - page_bits)) / subentries;
    using CachedPages = std::array<CounterEntry, num_counter_entries>;
    std::unique_ptr<CachedPages> cached_pages;
    std::mutex counter_guard;
    std::mutex mapping_guard;
};

} // namespace Core