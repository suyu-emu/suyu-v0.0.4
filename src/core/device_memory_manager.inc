// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <atomic>
#include <limits>
#include <memory>
#include <type_traits>

#include "common/address_space.h"
#include "common/address_space.inc"
#include "common/alignment.h"
#include "common/assert.h"
#include "common/div_ceil.h"
#include "common/scope_exit.h"
#include "core/device_memory.h"
#include "core/device_memory_manager.h"
#include "core/memory.h"

namespace Core {

struct EmptyAllocator {
    EmptyAllocator([[maybe_unused]] DAddr address) {}
};

template <typename DTraits>
struct DeviceMemoryManagerAllocator {
    static constexpr bool supports_pinning = DTraits::supports_pinning;
    static constexpr size_t device_virtual_bits = DTraits::device_virtual_bits;
    static constexpr size_t pin_bits = 32;
    static constexpr DAddr first_address = 1ULL << Memory::YUZU_PAGEBITS;
    static constexpr DAddr max_pin_area = supports_pinning ? 1ULL << pin_bits : first_address;
    static constexpr DAddr max_device_area = 1ULL << device_virtual_bits;

    DeviceMemoryManagerAllocator()
        : pin_allocator(first_address),
          main_allocator(supports_pinning ? 1ULL << pin_bits : first_address) {}

    std::conditional_t<supports_pinning, Common::FlatAllocator<DAddr, 0, pin_bits>, EmptyAllocator>
        pin_allocator;
    Common::FlatAllocator<DAddr, 0, device_virtual_bits> main_allocator;

    /// Returns true when vaddr -> vaddr+size is fully contained in the buffer
    template <bool pin_area>
    [[nodiscard]] bool IsInBounds(VAddr addr, u64 size) const noexcept {
        if constexpr (pin_area) {
            return addr >= 0 && addr + size <= max_pin_area;
        } else {
            return addr >= max_pin_area && addr + size <= max_device_area;
        }
    }

    DAddr Allocate(size_t size) {
        return main_allocator.Allocate(size);
    }

    DAddr AllocatePinned(size_t size) {
        if constexpr (supports_pinning) {
            return pin_allocator.Allocate(size);
        } else {
            return DAddr{};
        }
    }

    void DoInRange(DAddr address, size_t size, auto pin_func, auto main_func) {
        if (IsInBounds<true>(address, size)) {
            pin_func(address, size);
            return;
        }
        if (IsInBounds<false>(address, size)) {
            main_func(address, size);
            return;
        }
        DAddr end_size = address + size - max_pin_area;
        DAddr end_size2 = max_pin_area - address;
        pin_func(address, end_size2);
        main_func(max_pin_area, end_size);
    }

    void AllocateFixed(DAddr b_address, size_t b_size) {
        if constexpr (supports_pinning) {
            DoInRange(
                b_address, b_size,
                [this](DAddr address, size_t size) { pin_allocator.AllocateFixed(address, size); },
                [this](DAddr address, size_t size) {
                    main_allocator.AllocateFixed(address, size);
                });
        } else {
            main_allocator.AllocateFixed(b_address, b_size);
        }
    }

    void Free(DAddr b_address, size_t b_size) {
        if constexpr (supports_pinning) {
            DoInRange(
                b_address, b_size,
                [this](DAddr address, size_t size) { pin_allocator.Free(address, size); },
                [this](DAddr address, size_t size) { main_allocator.Free(address, size); });
        } else {
            main_allocator.Free(b_address, b_size);
        }
    }
};

template <typename Traits>
DeviceMemoryManager<Traits>::DeviceMemoryManager(const DeviceMemory& device_memory_)
    : physical_base{reinterpret_cast<const uintptr_t>(device_memory_.buffer.BackingBasePointer())},
      interface{nullptr}, compressed_physical_ptr(device_as_size >> Memory::YUZU_PAGEBITS),
      compressed_device_addr(1ULL << (physical_max_bits - Memory::YUZU_PAGEBITS)) {
    impl = std::make_unique<DeviceMemoryManagerAllocator<Traits>>();
    cached_pages = std::make_unique<CachedPages>();
}

template <typename Traits>
DeviceMemoryManager<Traits>::~DeviceMemoryManager() = default;

template <typename Traits>
void DeviceMemoryManager<Traits>::BindInterface(DeviceInterface* interface_) {
    interface = interface_;
}

template <typename Traits>
DAddr DeviceMemoryManager<Traits>::Allocate(size_t size) {
    return impl->Allocate(size);
}

template <typename Traits>
void DeviceMemoryManager<Traits>::AllocateFixed(DAddr start, size_t size) {
    return impl->AllocateFixed(start, size);
}

template <typename Traits>
DAddr DeviceMemoryManager<Traits>::AllocatePinned(size_t size) {
    return impl->AllocatePinned(size);
}

template <typename Traits>
void DeviceMemoryManager<Traits>::Free(DAddr start, size_t size) {
    impl->Free(start, size);
}

template <typename Traits>
void DeviceMemoryManager<Traits>::Map(DAddr address, VAddr virtual_address, size_t size,
                                      size_t process_id) {
    Core::Memory::Memory* process_memory = registered_processes[process_id];
    size_t start_page_d = address >> Memory::YUZU_PAGEBITS;
    size_t num_pages = Common::AlignUp(size, Memory::YUZU_PAGESIZE) >> Memory::YUZU_PAGEBITS;
    std::atomic_thread_fence(std::memory_order_acquire);
    for (size_t i = 0; i < num_pages; i++) {
        const VAddr new_vaddress = virtual_address + i * Memory::YUZU_PAGESIZE;
        auto* ptr = process_memory->GetPointer(Common::ProcessAddress(new_vaddress));
        if (ptr == nullptr) [[unlikely]] {
            compressed_physical_ptr[start_page_d + i] = 0;
            continue;
        }
        auto phys_addr = static_cast<u32>(GetRawPhysicalAddr(ptr) >> Memory::YUZU_PAGEBITS) + 1U;
        compressed_physical_ptr[start_page_d + i] = phys_addr;
        compressed_device_addr[phys_addr - 1U] = static_cast<u32>(start_page_d + i);
        InsertCPUBacking(start_page_d + i, new_vaddress, process_id);
    }
    std::atomic_thread_fence(std::memory_order_release);
}

template <typename Traits>
void DeviceMemoryManager<Traits>::Unmap(DAddr address, size_t size) {
    size_t start_page_d = address >> Memory::YUZU_PAGEBITS;
    size_t num_pages = Common::AlignUp(size, Memory::YUZU_PAGESIZE) >> Memory::YUZU_PAGEBITS;
    std::atomic_thread_fence(std::memory_order_acquire);
    for (size_t i = 0; i < num_pages; i++) {
        auto phys_addr = compressed_physical_ptr[start_page_d + i];
        compressed_physical_ptr[start_page_d + i] = 0;
        cpu_backing_address[start_page_d + i] = 0;
        if (phys_addr != 0) {
            compressed_device_addr[phys_addr - 1] = 0;
        }
    }
    std::atomic_thread_fence(std::memory_order_release);
}

template <typename Traits>
template <typename T>
T* DeviceMemoryManager<Traits>::GetPointer(DAddr address) {
    const size_t index = address >> Memory::YUZU_PAGEBITS;
    const size_t offset = address & Memory::YUZU_PAGEMASK;
    auto phys_addr = compressed_physical_ptr[index];
    if (phys_addr == 0) [[unlikely]] {
        return nullptr;
    }
    return GetPointerFromRaw<T>(
        static_cast<PAddr>(((phys_addr - 1) << Memory::YUZU_PAGEBITS) + offset));
}

template <typename Traits>
template <typename T>
const T* DeviceMemoryManager<Traits>::GetPointer(DAddr address) const {
    const size_t index = address >> Memory::YUZU_PAGEBITS;
    const size_t offset = address & Memory::YUZU_PAGEMASK;
    auto phys_addr = compressed_physical_ptr[index];
    if (phys_addr == 0) [[unlikely]] {
        return nullptr;
    }
    return GetPointerFromRaw<T>(
        static_cast<PAddr>(((phys_addr - 1) << Memory::YUZU_PAGEBITS) + offset));
}

template <typename Traits>
template <typename T>
void DeviceMemoryManager<Traits>::Write(DAddr address, T value) {
    T* ptr = GetPointer<T>(address);
    if (!ptr) [[unlikely]] {
        return;
    }
    std::memcpy(ptr, &value, sizeof(T));
}

template <typename Traits>
template <typename T>
T DeviceMemoryManager<Traits>::Read(DAddr address) const {
    const T* ptr = GetPointer<T>(address);
    T result{};
    if (!ptr) [[unlikely]] {
        return result;
    }
    std::memcpy(&result, ptr, sizeof(T));
    return result;
}

template <typename Traits>
void DeviceMemoryManager<Traits>::WalkBlock(DAddr addr, std::size_t size, auto on_unmapped,
                                            auto on_memory, auto increment) {
    std::size_t remaining_size = size;
    std::size_t page_index = addr >> Memory::YUZU_PAGEBITS;
    std::size_t page_offset = addr & Memory::YUZU_PAGEMASK;

    while (remaining_size) {
        const std::size_t copy_amount =
            std::min(static_cast<std::size_t>(Memory::YUZU_PAGESIZE) - page_offset, remaining_size);
        const auto current_vaddr =
            static_cast<u64>((page_index << Memory::YUZU_PAGEBITS) + page_offset);
        SCOPE_EXIT({
            page_index++;
            page_offset = 0;
            increment(copy_amount);
            remaining_size -= copy_amount;
        });

        auto phys_addr = compressed_physical_ptr[page_index];
        if (phys_addr == 0) {
            on_unmapped(copy_amount, current_vaddr);
            continue;
        }
        auto* mem_ptr = GetPointerFromRaw<u8>(
            static_cast<PAddr>(((phys_addr - 1) << Memory::YUZU_PAGEBITS) + page_offset));
        on_memory(copy_amount, mem_ptr);
    }
}

template <typename Traits>
void DeviceMemoryManager<Traits>::ReadBlock(DAddr address, void* dest_pointer, size_t size) {
    WalkBlock(
        address, size,
        [&](size_t copy_amount, DAddr current_vaddr) {
            LOG_ERROR(
                HW_Memory,
                "Unmapped Device ReadBlock @ 0x{:016X} (start address = 0x{:016X}, size = {})",
                current_vaddr, address, size);
            std::memset(dest_pointer, 0, copy_amount);
        },
        [&](size_t copy_amount, const u8* const src_ptr) {
            std::memcpy(dest_pointer, src_ptr, copy_amount);
        },
        [&](const std::size_t copy_amount) {
            dest_pointer = static_cast<u8*>(dest_pointer) + copy_amount;
        });
}

template <typename Traits>
void DeviceMemoryManager<Traits>::WriteBlock(DAddr address, void* src_pointer, size_t size) {
    WalkBlock(
        address, size,
        [&](size_t copy_amount, DAddr current_vaddr) {
            LOG_ERROR(
                HW_Memory,
                "Unmapped Device WriteBlock @ 0x{:016X} (start address = 0x{:016X}, size = {})",
                current_vaddr, address, size);
        },
        [&](size_t copy_amount, u8* const dst_ptr) {
            std::memcpy(dst_ptr, src_pointer, copy_amount);
        },
        [&](const std::size_t copy_amount) {
            src_pointer = static_cast<u8*>(src_pointer) + copy_amount;
        });
}

template <typename Traits>
size_t DeviceMemoryManager<Traits>::RegisterProcess(Memory::Memory* memory_interface) {
    size_t new_id;
    if (!id_pool.empty()) {
        new_id = id_pool.front();
        id_pool.pop_front();
        registered_processes[new_id] = memory_interface;
    } else {
        registered_processes.emplace_back(memory_interface);
        new_id = registered_processes.size() - 1U;
    }
    return new_id;
}

template <typename Traits>
void DeviceMemoryManager<Traits>::UnregisterProcess(size_t id) {
    registered_processes[id] = nullptr;
    id_pool.push_front(id);
}

template <typename Traits>
void DeviceMemoryManager<Traits>::UpdatePagesCachedCount(DAddr addr, size_t size, s32 delta) {
    u64 uncache_begin = 0;
    u64 cache_begin = 0;
    u64 uncache_bytes = 0;
    u64 cache_bytes = 0;
    const auto* MarkRegionCaching = &DeviceMemoryManager<Traits>::DeviceMethods::MarkRegionCaching;

    std::atomic_thread_fence(std::memory_order_acquire);
    const size_t page_end = Common::DivCeil(addr + size, Memory::YUZU_PAGESIZE);
    size_t page = addr >> Memory::YUZU_PAGEBITS;
    auto [process_id, base_vaddress] = ExtractCPUBacking(page);
    size_t vpage = base_vaddress >> Memory::YUZU_PAGEBITS;
    auto* memory_interface = registered_processes[process_id];
    for (; page != page_end; ++page) {
        std::atomic_uint16_t& count = cached_pages->at(page >> 2).Count(page);

        if (delta > 0) {
            ASSERT_MSG(count.load(std::memory_order::relaxed) < std::numeric_limits<u16>::max(),
                       "Count may overflow!");
        } else if (delta < 0) {
            ASSERT_MSG(count.load(std::memory_order::relaxed) > 0, "Count may underflow!");
        } else {
            ASSERT_MSG(false, "Delta must be non-zero!");
        }

        // Adds or subtracts 1, as count is a unsigned 8-bit value
        count.fetch_add(static_cast<u16>(delta), std::memory_order_release);

        // Assume delta is either -1 or 1
        if (count.load(std::memory_order::relaxed) == 0) {
            if (uncache_bytes == 0) {
                uncache_begin = vpage;
            }
            uncache_bytes += Memory::YUZU_PAGESIZE;
        } else if (uncache_bytes > 0) {
            MarkRegionCaching(memory_interface, uncache_begin << Memory::YUZU_PAGEBITS,
                              uncache_bytes, false);
            uncache_bytes = 0;
        }
        if (count.load(std::memory_order::relaxed) == 1 && delta > 0) {
            if (cache_bytes == 0) {
                cache_begin = vpage;
            }
            cache_bytes += Memory::YUZU_PAGESIZE;
        } else if (cache_bytes > 0) {
            MarkRegionCaching(memory_interface, cache_begin << Memory::YUZU_PAGEBITS, cache_bytes,
                              true);
            cache_bytes = 0;
        }
        vpage++;
    }
    if (uncache_bytes > 0) {
        MarkRegionCaching(memory_interface, uncache_begin << Memory::YUZU_PAGEBITS, uncache_bytes,
                          false);
    }
    if (cache_bytes > 0) {
        MarkRegionCaching(memory_interface, cache_begin << Memory::YUZU_PAGEBITS, cache_bytes,
                          true);
    }
}

} // namespace Core