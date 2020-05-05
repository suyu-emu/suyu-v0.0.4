// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <mutex>

#include "common/common_types.h"
#include "common/page_table.h"
#include "core/file_sys/program_metadata.h"
#include "core/hle/kernel/memory/memory_block.h"
#include "core/hle/kernel/memory/memory_manager.h"
#include "core/hle/result.h"

namespace Core {
class System;
}

namespace Kernel::Memory {

class MemoryBlockManager;

class PageTable final : NonCopyable {
public:
    explicit PageTable(Core::System& system);

    ResultCode InitializeForProcess(FileSys::ProgramAddressSpaceType as_type, bool enable_aslr,
                                    VAddr code_addr, std::size_t code_size,
                                    Memory::MemoryManager::Pool pool);
    ResultCode MapProcessCode(VAddr addr, std::size_t pages_count, MemoryState state,
                              MemoryPermission perm);
    ResultCode MapProcessCodeMemory(VAddr dst_addr, VAddr src_addr, std::size_t size);
    ResultCode UnmapProcessCodeMemory(VAddr dst_addr, VAddr src_addr, std::size_t size);
    ResultCode MapPhysicalMemory(VAddr addr, std::size_t size);
    ResultCode UnmapPhysicalMemory(VAddr addr, std::size_t size);
    ResultCode UnmapMemory(VAddr addr, std::size_t size);
    ResultCode Map(VAddr dst_addr, VAddr src_addr, std::size_t size);
    ResultCode Unmap(VAddr dst_addr, VAddr src_addr, std::size_t size);
    ResultCode MapPages(VAddr addr, PageLinkedList& page_linked_list, MemoryState state,
                        MemoryPermission perm);
    ResultCode SetCodeMemoryPermission(VAddr addr, std::size_t size, MemoryPermission perm);
    MemoryInfo QueryInfo(VAddr addr);
    ResultCode ReserveTransferMemory(VAddr addr, std::size_t size, MemoryPermission perm);
    ResultCode ResetTransferMemory(VAddr addr, std::size_t size);
    ResultCode SetMemoryAttribute(VAddr addr, std::size_t size, MemoryAttribute mask,
                                  MemoryAttribute value);
    ResultCode SetHeapCapacity(std::size_t new_heap_capacity);
    ResultVal<VAddr> SetHeapSize(std::size_t size);
    ResultVal<VAddr> AllocateAndMapMemory(std::size_t needed_num_pages, std::size_t align,
                                          bool is_map_only, VAddr region_start,
                                          std::size_t region_num_pages, MemoryState state,
                                          MemoryPermission perm, PAddr map_addr = 0);
    ResultCode LockForDeviceAddressSpace(VAddr addr, std::size_t size);
    ResultCode UnlockForDeviceAddressSpace(VAddr addr, std::size_t size);

    Common::PageTable& PageTableImpl() {
        return page_table_impl;
    }

    const Common::PageTable& PageTableImpl() const {
        return page_table_impl;
    }

private:
    enum class OperationType : u32 {
        Map,
        MapGroup,
        Unmap,
        ChangePermissions,
        ChangePermissionsAndRefresh,
    };

    static constexpr MemoryAttribute DefaultMemoryIgnoreAttr =
        MemoryAttribute::DontCareMask | MemoryAttribute::IpcLocked | MemoryAttribute::DeviceShared;

    ResultCode InitializeMemoryLayout(VAddr start, VAddr end);
    ResultCode MapPages(VAddr addr, const PageLinkedList& page_linked_list, MemoryPermission perm);
    void MapPhysicalMemory(PageLinkedList& page_linked_list, VAddr start, VAddr end);
    bool IsRegionMapped(VAddr address, u64 size);
    bool IsRegionContiguous(VAddr addr, u64 size) const;
    void AddRegionToPages(VAddr start, std::size_t num_pages, PageLinkedList& page_linked_list);
    MemoryInfo QueryInfoImpl(VAddr addr);
    VAddr AllocateVirtualMemory(VAddr start, std::size_t region_num_pages, u64 needed_num_pages,
                                std::size_t align);
    ResultCode Operate(VAddr addr, std::size_t num_pages, const PageLinkedList& page_group,
                       OperationType operation);
    ResultCode Operate(VAddr addr, std::size_t num_pages, MemoryPermission perm,
                       OperationType operation, PAddr map_addr = 0);
    constexpr VAddr GetRegionAddress(MemoryState state) const;
    constexpr std::size_t GetRegionSize(MemoryState state) const;
    constexpr bool CanContain(VAddr addr, std::size_t size, MemoryState state) const;

    constexpr ResultCode CheckMemoryState(const MemoryInfo& info, MemoryState state_mask,
                                          MemoryState state, MemoryPermission perm_mask,
                                          MemoryPermission perm, MemoryAttribute attr_mask,
                                          MemoryAttribute attr) const;
    ResultCode CheckMemoryState(MemoryState* out_state, MemoryPermission* out_perm,
                                MemoryAttribute* out_attr, VAddr addr, std::size_t size,
                                MemoryState state_mask, MemoryState state,
                                MemoryPermission perm_mask, MemoryPermission perm,
                                MemoryAttribute attr_mask, MemoryAttribute attr,
                                MemoryAttribute ignore_attr = DefaultMemoryIgnoreAttr);
    ResultCode CheckMemoryState(VAddr addr, std::size_t size, MemoryState state_mask,
                                MemoryState state, MemoryPermission perm_mask,
                                MemoryPermission perm, MemoryAttribute attr_mask,
                                MemoryAttribute attr,
                                MemoryAttribute ignore_attr = DefaultMemoryIgnoreAttr) {
        return CheckMemoryState(nullptr, nullptr, nullptr, addr, size, state_mask, state, perm_mask,
                                perm, attr_mask, attr, ignore_attr);
    }

    std::recursive_mutex page_table_lock;
    std::unique_ptr<MemoryBlockManager> block_manager;

public:
    constexpr VAddr GetAddressSpaceStart() const {
        return address_space_start;
    }
    constexpr VAddr GetAddressSpaceEnd() const {
        return address_space_end;
    }
    constexpr std::size_t GetAddressSpaceSize() const {
        return address_space_end - address_space_start;
    }
    constexpr VAddr GetHeapRegionStart() const {
        return heap_region_start;
    }
    constexpr VAddr GetHeapRegionEnd() const {
        return heap_region_end;
    }
    constexpr std::size_t GetHeapRegionSize() const {
        return heap_region_end - heap_region_start;
    }
    constexpr VAddr GetAliasRegionStart() const {
        return alias_region_start;
    }
    constexpr VAddr GetAliasRegionEnd() const {
        return alias_region_end;
    }
    constexpr std::size_t GetAliasRegionSize() const {
        return alias_region_end - alias_region_start;
    }
    constexpr VAddr GetStackRegionStart() const {
        return stack_region_start;
    }
    constexpr VAddr GetStackRegionEnd() const {
        return stack_region_end;
    }
    constexpr std::size_t GetStackRegionSize() const {
        return stack_region_end - stack_region_start;
    }
    constexpr VAddr GetKernelMapRegionStart() const {
        return kernel_map_region_start;
    }
    constexpr VAddr GetKernelMapRegionEnd() const {
        return kernel_map_region_end;
    }
    constexpr VAddr GetCodeRegionStart() const {
        return code_region_start;
    }
    constexpr VAddr GetCodeRegionEnd() const {
        return code_region_end;
    }
    constexpr VAddr GetAliasCodeRegionStart() const {
        return alias_code_region_start;
    }
    constexpr VAddr GetAliasCodeRegionSize() const {
        return alias_code_region_end - alias_code_region_start;
    }
    constexpr std::size_t GetAddressSpaceWidth() const {
        return address_space_width;
    }
    constexpr std::size_t GetHeapSize() {
        return current_heap_addr - heap_region_start;
    }
    constexpr std::size_t GetTotalHeapSize() {
        return GetHeapSize() + physical_memory_usage;
    }
    constexpr bool IsInsideAddressSpace(VAddr address, std::size_t size) const {
        return address_space_start <= address && address + size - 1 <= address_space_end - 1;
    }
    constexpr bool IsOutsideAliasRegion(VAddr address, std::size_t size) const {
        return alias_region_start > address || address + size - 1 > alias_region_end - 1;
    }
    constexpr bool IsOutsideStackRegion(VAddr address, std::size_t size) const {
        return stack_region_start > address || address + size - 1 > stack_region_end - 1;
    }
    constexpr bool IsInvalidRegion(VAddr address, std::size_t size) const {
        return address + size - 1 > GetAliasCodeRegionStart() + GetAliasCodeRegionSize() - 1;
    }
    constexpr bool IsInsideHeapRegion(VAddr address, std::size_t size) const {
        return address + size > heap_region_start && heap_region_end > address;
    }
    constexpr bool IsInsideAliasRegion(VAddr address, std::size_t size) const {
        return address + size > alias_region_start && alias_region_end > address;
    }
    constexpr bool IsOutsideASLRRegion(VAddr address, std::size_t size) const {
        if (IsInvalidRegion(address, size)) {
            return true;
        }
        if (IsInsideHeapRegion(address, size)) {
            return true;
        }
        if (IsInsideAliasRegion(address, size)) {
            return true;
        }
        return {};
    }
    constexpr bool IsInsideASLRRegion(VAddr address, std::size_t size) const {
        return !IsOutsideASLRRegion(address, size);
    }
    constexpr PAddr GetPhysicalAddr(VAddr addr) {
        return page_table_impl.backing_addr[addr >> Memory::PageBits] + addr;
    }

private:
    constexpr bool Contains(VAddr addr) const {
        return address_space_start <= addr && addr <= address_space_end - 1;
    }
    constexpr bool Contains(VAddr addr, std::size_t size) const {
        return address_space_start <= addr && addr < addr + size &&
               addr + size - 1 <= address_space_end - 1;
    }
    constexpr bool IsKernel() const {
        return is_kernel;
    }
    constexpr bool IsAslrEnabled() const {
        return is_aslr_enabled;
    }

    constexpr std::size_t GetNumGuardPages() const {
        return IsKernel() ? 1 : 4;
    }

    constexpr bool ContainsPages(VAddr addr, std::size_t num_pages) const {
        return (address_space_start <= addr) &&
               (num_pages <= (address_space_end - address_space_start) / PageSize) &&
               (addr + num_pages * PageSize - 1 <= address_space_end - 1);
    }

private:
    VAddr address_space_start{};
    VAddr address_space_end{};
    VAddr heap_region_start{};
    VAddr heap_region_end{};
    VAddr current_heap_end{};
    VAddr alias_region_start{};
    VAddr alias_region_end{};
    VAddr stack_region_start{};
    VAddr stack_region_end{};
    VAddr kernel_map_region_start{};
    VAddr kernel_map_region_end{};
    VAddr code_region_start{};
    VAddr code_region_end{};
    VAddr alias_code_region_start{};
    VAddr alias_code_region_end{};
    VAddr current_heap_addr{};

    std::size_t heap_capacity{};
    std::size_t physical_memory_usage{};
    std::size_t max_heap_size{};
    std::size_t max_physical_memory_size{};
    std::size_t address_space_width{};

    bool is_kernel{};
    bool is_aslr_enabled{};

    MemoryManager::Pool memory_pool{MemoryManager::Pool::Application};

    Common::PageTable page_table_impl;

    Core::System& system;
};

} // namespace Kernel::Memory
