// SPDX-FileCopyrightText: 2015 Citra Emulator Project
// SPDX-FileCopyrightText: 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <cstring>
#include <mutex>
#include <span>
#include <vector>

#include "common/assert.h"
#include "common/atomic_ops.h"
#include "common/common_types.h"
#include "common/heap_tracker.h"
#include "common/logging/log.h"
#include "common/page_table.h"
#include "common/scope_exit.h"
#include "common/settings.h"
#include "common/swap.h"
#include "core/core.h"
#include "core/device_memory.h"
#include "core/gpu_dirty_memory_manager.h"
#include "core/hardware_properties.h"
#include "core/hle/kernel/k_page_table.h"
#include "core/hle/kernel/k_process.h"
#include "core/memory.h"
#include "video_core/gpu.h"
#include "video_core/host1x/gpu_device_memory_manager.h"
#include "video_core/host1x/host1x.h"
#include "video_core/rasterizer_download_area.h"

namespace Core::Memory {

namespace {

constexpr size_t PAGE_SIZE = 0x1000;
constexpr size_t PAGE_BITS = 12;
constexpr size_t PAGE_MASK = PAGE_SIZE - 1;

inline bool AddressSpaceContains(const Common::PageTable& table, const Common::ProcessAddress addr,
                                 const std::size_t size) {
    const Common::ProcessAddress max_addr = 1ULL << table.GetAddressSpaceBits();
    return addr + size >= addr && addr + size <= max_addr;
}

} // Anonymous namespace

struct Memory::Impl {
    explicit Impl(Core::System& system_) : system{system_} {}

    void SetCurrentPageTable(Kernel::KProcess& process) {
        current_page_table = &process.GetPageTable().GetImpl();

        if (process.IsApplication() && Settings::IsFastmemEnabled()) {
            current_page_table->fastmem_arena = system.DeviceMemory().buffer.VirtualBasePointer();
        } else {
            current_page_table->fastmem_arena = nullptr;
        }

#ifdef __linux__
        heap_tracker.emplace(system.DeviceMemory().buffer);
        buffer = std::addressof(*heap_tracker);
#else
        buffer = std::addressof(system.DeviceMemory().buffer);
#endif
    }

    void MapMemoryRegion(Common::PageTable& page_table, Common::ProcessAddress base, u64 size,
                         Common::PhysicalAddress target, Common::MemoryPermission perms,
                         bool separate_heap) {
        ASSERT_MSG((size & PAGE_MASK) == 0, "non-page aligned size: {:016X}", size);
        ASSERT_MSG((base & PAGE_MASK) == 0, "non-page aligned base: {:016X}", GetInteger(base));
        ASSERT_MSG(target >= DramMemoryMap::Base, "Out of bounds target: {:016X}",
                   GetInteger(target));
        MapPages(page_table, base / PAGE_SIZE, size / PAGE_SIZE, target, Common::PageType::Memory);

        if (current_page_table->fastmem_arena) {
            buffer->Map(GetInteger(base), GetInteger(target) - DramMemoryMap::Base, size, perms,
                        separate_heap);
        }
    }

    void UnmapRegion(Common::PageTable& page_table, Common::ProcessAddress base, u64 size,
                     bool separate_heap) {
        ASSERT_MSG((size & PAGE_MASK) == 0, "non-page aligned size: {:016X}", size);
        ASSERT_MSG((base & PAGE_MASK) == 0, "non-page aligned base: {:016X}", GetInteger(base));
        MapPages(page_table, base / PAGE_SIZE, size / PAGE_SIZE, 0, Common::PageType::Unmapped);

        if (current_page_table->fastmem_arena) {
            buffer->Unmap(GetInteger(base), size, separate_heap);
        }
    }

    void ProtectRegion(Common::PageTable& page_table, VAddr vaddr, u64 size,
                       Common::MemoryPermission perms) {
        ASSERT_MSG((size & PAGE_MASK) == 0, "non-page aligned size: {:016X}", size);
        ASSERT_MSG((vaddr & PAGE_MASK) == 0, "non-page aligned base: {:016X}", vaddr);

        if (!current_page_table->fastmem_arena) {
            return;
        }

        for (u64 addr = vaddr; addr < vaddr + size; addr += PAGE_SIZE) {
            const Common::PageType page_type{
                current_page_table->pointers[addr >> PAGE_BITS].Type()};
            if (page_type != Common::PageType::RasterizerCachedMemory) {
                buffer->Protect(addr, PAGE_SIZE, perms);
            }
        }
    }

    u8* GetPointerFromRasterizerCachedMemory(u64 vaddr) const {
        const Common::PhysicalAddress paddr{
            current_page_table->backing_addr[vaddr >> PAGE_BITS]};

        if (!paddr) {
            return nullptr;
        }

        return system.DeviceMemory().GetPointer<u8>(paddr + vaddr);
    }

    u8 Read8(const Common::ProcessAddress addr) {
        return Read<u8>(addr);
    }

    u16 Read16(const Common::ProcessAddress addr) {
        if ((addr & 1) == 0) {
            return Read<u16_le>(addr);
        } else {
            return Read<u8>(addr) | static_cast<u16>(Read<u8>(addr + sizeof(u8))) << 8;
        }
    }

    u32 Read32(const Common::ProcessAddress addr) {
        if ((addr & 3) == 0) {
            return Read<u32_le>(addr);
        } else {
            return Read16(addr) | static_cast<u32>(Read16(addr + sizeof(u16))) << 16;
        }
    }

    u64 Read64(const Common::ProcessAddress addr) {
        if ((addr & 7) == 0) {
            return Read<u64_le>(addr);
        } else {
            return Read32(addr) | static_cast<u64>(Read32(addr + sizeof(u32))) << 32;
        }
    }

    void Write8(const Common::ProcessAddress addr, const u8 data) {
        Write<u8>(addr, data);
    }

    void Write16(const Common::ProcessAddress addr, const u16 data) {
        if ((addr & 1) == 0) {
            Write<u16_le>(addr, data);
        } else {
            Write<u8>(addr, static_cast<u8>(data));
            Write<u8>(addr + sizeof(u8), static_cast<u8>(data >> 8));
        }
    }

    void Write32(const Common::ProcessAddress addr, const u32 data) {
        if ((addr & 3) == 0) {
            Write<u32_le>(addr, data);
        } else {
            Write16(addr, static_cast<u16>(data));
            Write16(addr + sizeof(u16), static_cast<u16>(data >> 16));
        }
    }

    void Write64(const Common::ProcessAddress addr, const u64 data) {
        if ((addr & 7) == 0) {
            Write<u64_le>(addr, data);
        } else {
            Write32(addr, static_cast<u32>(data));
            Write32(addr + sizeof(u32), static_cast<u32>(data >> 32));
        }
    }

    bool WriteExclusive8(const Common::ProcessAddress addr, const u8 data, const u8 expected) {
        return WriteExclusive<u8>(addr, data, expected);
    }

    bool WriteExclusive16(const Common::ProcessAddress addr, const u16 data, const u16 expected) {
        return WriteExclusive<u16_le>(addr, data, expected);
    }

    bool WriteExclusive32(const Common::ProcessAddress addr, const u32 data, const u32 expected) {
        return WriteExclusive<u32_le>(addr, data, expected);
    }

    bool WriteExclusive64(const Common::ProcessAddress addr, const u64 data, const u64 expected) {
        return WriteExclusive<u64_le>(addr, data, expected);
    }

    std::string ReadCString(Common::ProcessAddress vaddr, std::size_t max_length) {
        std::string string;
        string.reserve(max_length);
        for (std::size_t i = 0; i < max_length; ++i) {
            const char c = Read<char>(vaddr);
            if (c == '\0') {
                break;
            }
            string.push_back(c);
            ++vaddr;
        }
        string.shrink_to_fit();
        return string;
    }

    template <typename T>
    T Read(const Common::ProcessAddress vaddr) {
        T value;
        const u8* const ptr = GetPointerFromRasterizerCachedMemory(GetInteger(vaddr));
        if (ptr) {
            std::memcpy(&value, ptr, sizeof(T));
        } else {
            LOG_ERROR(HW_Memory, "Unmapped Read{} @ 0x{:016X}", sizeof(T) * 8, GetInteger(vaddr));
            value = 0;
        }
        return value;
    }

    template <typename T>
    void Write(Common::ProcessAddress vaddr, const T data) {
        u8* const ptr = GetPointerFromRasterizerCachedMemory(GetInteger(vaddr));
        if (ptr) {
            std::memcpy(ptr, &data, sizeof(T));
            system.GPU().InvalidateRegion(GetInteger(vaddr), sizeof(T));
        } else {
            LOG_ERROR(HW_Memory, "Unmapped Write{} @ 0x{:016X} = 0x{:016X}", sizeof(T) * 8,
                      GetInteger(vaddr), static_cast<u64>(data));
        }
    }

    template <typename T>
    bool WriteExclusive(Common::ProcessAddress vaddr, const T data, const T expected) {
        u8* const ptr = GetPointerFromRasterizerCachedMemory(GetInteger(vaddr));
        if (ptr) {
            const bool result = Common::AtomicCompareAndSwap(reinterpret_cast<T*>(ptr), data, expected);
            if (result) {
                system.GPU().InvalidateRegion(GetInteger(vaddr), sizeof(T));
            }
            return result;
        } else {
            LOG_ERROR(HW_Memory, "Unmapped WriteExclusive{} @ 0x{:016X} = 0x{:016X}", sizeof(T) * 8,
                      GetInteger(vaddr), static_cast<u64>(data));
            return true;
        }
    }

    bool ReadBlock(const Common::ProcessAddress src_addr, void* dest_buffer,
                   const std::size_t size) {
        const u8* src_ptr = GetPointerFromRasterizerCachedMemory(GetInteger(src_addr));
        if (src_ptr) {
            std::memcpy(dest_buffer, src_ptr, size);
            return true;
        }
        LOG_ERROR(HW_Memory, "Unmapped ReadBlock @ 0x{:016X}", GetInteger(src_addr));
        return false;
    }

    bool WriteBlock(const Common::ProcessAddress dest_addr, const void* src_buffer,
                    const std::size_t size) {
        u8* const dest_ptr = GetPointerFromRasterizerCachedMemory(GetInteger(dest_addr));
        if (dest_ptr) {
            std::memcpy(dest_ptr, src_buffer, size);
            system.GPU().InvalidateRegion(GetInteger(dest_addr), size);
            return true;
        }
        LOG_ERROR(HW_Memory, "Unmapped WriteBlock @ 0x{:016X}", GetInteger(dest_addr));
        return false;
    }

    Core::System& system;
    Common::PageTable* current_page_table = nullptr;
    std::optional<Common::HeapTracker> heap_tracker;
#ifdef __linux__
    Common::HeapTracker* buffer{};
#else
    Common::HostMemory* buffer{};
#endif
};

Memory::Memory(Core::System& system_) : impl{std::make_unique<Impl>(system_)} {}

Memory::~Memory() = default;

void Memory::SetCurrentPageTable(Kernel::KProcess& process) {
    impl->SetCurrentPageTable(process);
}

void Memory::MapMemoryRegion(Common::PageTable& page_table, Common::ProcessAddress base, u64 size,
                             Common::PhysicalAddress target, Common::MemoryPermission perms,
                             bool separate_heap) {
    impl->MapMemoryRegion(page_table, base, size, target, perms, separate_heap);
}

void Memory::UnmapRegion(Common::PageTable& page_table, Common::ProcessAddress base, u64 size,
                         bool separate_heap) {
    impl->UnmapRegion(page_table, base, size, separate_heap);
}

void Memory::ProtectRegion(Common::PageTable& page_table, Common::ProcessAddress vaddr, u64 size,
                           Common::MemoryPermission perms) {
    impl->ProtectRegion(page_table, GetInteger(vaddr), size, perms);
}

bool Memory::IsValidVirtualAddress(const Common::ProcessAddress vaddr) const {
    const auto& page_table = *impl->current_page_table;
    const size_t page = vaddr >> PAGE_BITS;
    if (page >= page_table.pointers.size()) {
        return false;
    }
    const auto [pointer, type] = page_table.pointers[page].PointerType();
    return pointer != 0 || type == Common::PageType::RasterizerCachedMemory;
}

u8* Memory::GetPointer(Common::ProcessAddress vaddr) {
    return impl->GetPointerFromRasterizerCachedMemory(GetInteger(vaddr));
}

const u8* Memory::GetPointer(Common::ProcessAddress vaddr) const {
    return impl->GetPointerFromRasterizerCachedMemory(GetInteger(vaddr));
}

u8 Memory::Read8(const Common::ProcessAddress addr) {
    return impl->Read8(addr);
}

u16 Memory::Read16(const Common::ProcessAddress addr) {
    return impl->Read16(addr);
}

u32 Memory::Read32(const Common::ProcessAddress addr) {
    return impl->Read32(addr);
}

u64 Memory::Read64(const Common::ProcessAddress addr) {
    return impl->Read64(addr);
}

void Memory::Write8(Common::ProcessAddress addr, u8 data) {
    impl->Write8(addr, data);
}

void Memory::Write16(Common::ProcessAddress addr, u16 data) {
    impl->Write16(addr, data);
}

void Memory::Write32(Common::ProcessAddress addr, u32 data) {
    impl->Write32(addr, data);
}

void Memory::Write64(Common::ProcessAddress addr, u64 data) {
    impl->Write64(addr, data);
}

bool Memory::WriteExclusive8(Common::ProcessAddress addr, u8 data, u8 expected) {
    return impl->WriteExclusive8(addr, data, expected);
}

bool Memory::WriteExclusive16(Common::ProcessAddress addr, u16 data, u16 expected) {
    return impl->WriteExclusive16(addr, data, expected);
}

bool Memory::WriteExclusive32(Common::ProcessAddress addr, u32 data, u32 expected) {
    return impl->WriteExclusive32(addr, data, expected);
}

bool Memory::WriteExclusive64(Common::ProcessAddress addr, u64 data, u64 expected) {
    return impl->WriteExclusive64(addr, data, expected);
}

std::string Memory::ReadCString(Common::ProcessAddress vaddr, std::size_t max_length) {
    return impl->ReadCString(vaddr, max_length);
}

bool Memory::ReadBlock(const Common::ProcessAddress src_addr, void* dest_buffer,
                       const std::size_t size) {
    return impl->ReadBlock(src_addr, dest_buffer, size);
}

bool Memory::WriteBlock(const Common::ProcessAddress dest_addr, const void* src_buffer,
                        const std::size_t size) {
    return impl->WriteBlock(dest_addr, src_buffer, size);
}

} // namespace Core::Memory
