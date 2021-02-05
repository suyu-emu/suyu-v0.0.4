// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/alignment.h"
#include "common/assert.h"
#include "common/scope_exit.h"
#include "core/core.h"
#include "core/hle/kernel/k_scoped_resource_reservation.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/memory/address_space_info.h"
#include "core/hle/kernel/memory/memory_block.h"
#include "core/hle/kernel/memory/memory_block_manager.h"
#include "core/hle/kernel/memory/page_linked_list.h"
#include "core/hle/kernel/memory/page_table.h"
#include "core/hle/kernel/memory/system_control.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/svc_results.h"
#include "core/memory.h"

namespace Kernel::Memory {

namespace {

constexpr std::size_t GetAddressSpaceWidthFromType(FileSys::ProgramAddressSpaceType as_type) {
    switch (as_type) {
    case FileSys::ProgramAddressSpaceType::Is32Bit:
    case FileSys::ProgramAddressSpaceType::Is32BitNoMap:
        return 32;
    case FileSys::ProgramAddressSpaceType::Is36Bit:
        return 36;
    case FileSys::ProgramAddressSpaceType::Is39Bit:
        return 39;
    default:
        UNREACHABLE();
        return {};
    }
}

constexpr u64 GetAddressInRange(const MemoryInfo& info, VAddr addr) {
    if (info.GetAddress() < addr) {
        return addr;
    }
    return info.GetAddress();
}

constexpr std::size_t GetSizeInRange(const MemoryInfo& info, VAddr start, VAddr end) {
    std::size_t size{info.GetSize()};
    if (info.GetAddress() < start) {
        size -= start - info.GetAddress();
    }
    if (info.GetEndAddress() > end) {
        size -= info.GetEndAddress() - end;
    }
    return size;
}

} // namespace

PageTable::PageTable(Core::System& system) : system{system} {}

ResultCode PageTable::InitializeForProcess(FileSys::ProgramAddressSpaceType as_type,
                                           bool enable_aslr, VAddr code_addr, std::size_t code_size,
                                           Memory::MemoryManager::Pool pool) {

    const auto GetSpaceStart = [this](AddressSpaceInfo::Type type) {
        return AddressSpaceInfo::GetAddressSpaceStart(address_space_width, type);
    };
    const auto GetSpaceSize = [this](AddressSpaceInfo::Type type) {
        return AddressSpaceInfo::GetAddressSpaceSize(address_space_width, type);
    };

    //  Set our width and heap/alias sizes
    address_space_width = GetAddressSpaceWidthFromType(as_type);
    const VAddr start = 0;
    const VAddr end{1ULL << address_space_width};
    std::size_t alias_region_size{GetSpaceSize(AddressSpaceInfo::Type::Alias)};
    std::size_t heap_region_size{GetSpaceSize(AddressSpaceInfo::Type::Heap)};

    ASSERT(start <= code_addr);
    ASSERT(code_addr < code_addr + code_size);
    ASSERT(code_addr + code_size - 1 <= end - 1);

    // Adjust heap/alias size if we don't have an alias region
    if (as_type == FileSys::ProgramAddressSpaceType::Is32BitNoMap) {
        heap_region_size += alias_region_size;
        alias_region_size = 0;
    }

    // Set code regions and determine remaining
    constexpr std::size_t RegionAlignment{2 * 1024 * 1024};
    VAddr process_code_start{};
    VAddr process_code_end{};
    std::size_t stack_region_size{};
    std::size_t kernel_map_region_size{};

    if (address_space_width == 39) {
        alias_region_size = GetSpaceSize(AddressSpaceInfo::Type::Alias);
        heap_region_size = GetSpaceSize(AddressSpaceInfo::Type::Heap);
        stack_region_size = GetSpaceSize(AddressSpaceInfo::Type::Stack);
        kernel_map_region_size = GetSpaceSize(AddressSpaceInfo::Type::Is32Bit);
        code_region_start = GetSpaceStart(AddressSpaceInfo::Type::Large64Bit);
        code_region_end = code_region_start + GetSpaceSize(AddressSpaceInfo::Type::Large64Bit);
        alias_code_region_start = code_region_start;
        alias_code_region_end = code_region_end;
        process_code_start = Common::AlignDown(code_addr, RegionAlignment);
        process_code_end = Common::AlignUp(code_addr + code_size, RegionAlignment);
    } else {
        stack_region_size = 0;
        kernel_map_region_size = 0;
        code_region_start = GetSpaceStart(AddressSpaceInfo::Type::Is32Bit);
        code_region_end = code_region_start + GetSpaceSize(AddressSpaceInfo::Type::Is32Bit);
        stack_region_start = code_region_start;
        alias_code_region_start = code_region_start;
        alias_code_region_end = GetSpaceStart(AddressSpaceInfo::Type::Small64Bit) +
                                GetSpaceSize(AddressSpaceInfo::Type::Small64Bit);
        stack_region_end = code_region_end;
        kernel_map_region_start = code_region_start;
        kernel_map_region_end = code_region_end;
        process_code_start = code_region_start;
        process_code_end = code_region_end;
    }

    // Set other basic fields
    is_aslr_enabled = enable_aslr;
    address_space_start = start;
    address_space_end = end;
    is_kernel = false;

    // Determine the region we can place our undetermineds in
    VAddr alloc_start{};
    std::size_t alloc_size{};
    if ((process_code_start - code_region_start) >= (end - process_code_end)) {
        alloc_start = code_region_start;
        alloc_size = process_code_start - code_region_start;
    } else {
        alloc_start = process_code_end;
        alloc_size = end - process_code_end;
    }
    const std::size_t needed_size{
        (alias_region_size + heap_region_size + stack_region_size + kernel_map_region_size)};
    if (alloc_size < needed_size) {
        UNREACHABLE();
        return ResultOutOfMemory;
    }

    const std::size_t remaining_size{alloc_size - needed_size};

    // Determine random placements for each region
    std::size_t alias_rnd{}, heap_rnd{}, stack_rnd{}, kmap_rnd{};
    if (enable_aslr) {
        alias_rnd = SystemControl::GenerateRandomRange(0, remaining_size / RegionAlignment) *
                    RegionAlignment;
        heap_rnd = SystemControl::GenerateRandomRange(0, remaining_size / RegionAlignment) *
                   RegionAlignment;
        stack_rnd = SystemControl::GenerateRandomRange(0, remaining_size / RegionAlignment) *
                    RegionAlignment;
        kmap_rnd = SystemControl::GenerateRandomRange(0, remaining_size / RegionAlignment) *
                   RegionAlignment;
    }

    // Setup heap and alias regions
    alias_region_start = alloc_start + alias_rnd;
    alias_region_end = alias_region_start + alias_region_size;
    heap_region_start = alloc_start + heap_rnd;
    heap_region_end = heap_region_start + heap_region_size;

    if (alias_rnd <= heap_rnd) {
        heap_region_start += alias_region_size;
        heap_region_end += alias_region_size;
    } else {
        alias_region_start += heap_region_size;
        alias_region_end += heap_region_size;
    }

    // Setup stack region
    if (stack_region_size) {
        stack_region_start = alloc_start + stack_rnd;
        stack_region_end = stack_region_start + stack_region_size;

        if (alias_rnd < stack_rnd) {
            stack_region_start += alias_region_size;
            stack_region_end += alias_region_size;
        } else {
            alias_region_start += stack_region_size;
            alias_region_end += stack_region_size;
        }

        if (heap_rnd < stack_rnd) {
            stack_region_start += heap_region_size;
            stack_region_end += heap_region_size;
        } else {
            heap_region_start += stack_region_size;
            heap_region_end += stack_region_size;
        }
    }

    // Setup kernel map region
    if (kernel_map_region_size) {
        kernel_map_region_start = alloc_start + kmap_rnd;
        kernel_map_region_end = kernel_map_region_start + kernel_map_region_size;

        if (alias_rnd < kmap_rnd) {
            kernel_map_region_start += alias_region_size;
            kernel_map_region_end += alias_region_size;
        } else {
            alias_region_start += kernel_map_region_size;
            alias_region_end += kernel_map_region_size;
        }

        if (heap_rnd < kmap_rnd) {
            kernel_map_region_start += heap_region_size;
            kernel_map_region_end += heap_region_size;
        } else {
            heap_region_start += kernel_map_region_size;
            heap_region_end += kernel_map_region_size;
        }

        if (stack_region_size) {
            if (stack_rnd < kmap_rnd) {
                kernel_map_region_start += stack_region_size;
                kernel_map_region_end += stack_region_size;
            } else {
                stack_region_start += kernel_map_region_size;
                stack_region_end += kernel_map_region_size;
            }
        }
    }

    // Set heap members
    current_heap_end = heap_region_start;
    max_heap_size = 0;
    max_physical_memory_size = 0;

    // Ensure that we regions inside our address space
    auto IsInAddressSpace = [&](VAddr addr) {
        return address_space_start <= addr && addr <= address_space_end;
    };
    ASSERT(IsInAddressSpace(alias_region_start));
    ASSERT(IsInAddressSpace(alias_region_end));
    ASSERT(IsInAddressSpace(heap_region_start));
    ASSERT(IsInAddressSpace(heap_region_end));
    ASSERT(IsInAddressSpace(stack_region_start));
    ASSERT(IsInAddressSpace(stack_region_end));
    ASSERT(IsInAddressSpace(kernel_map_region_start));
    ASSERT(IsInAddressSpace(kernel_map_region_end));

    // Ensure that we selected regions that don't overlap
    const VAddr alias_start{alias_region_start};
    const VAddr alias_last{alias_region_end - 1};
    const VAddr heap_start{heap_region_start};
    const VAddr heap_last{heap_region_end - 1};
    const VAddr stack_start{stack_region_start};
    const VAddr stack_last{stack_region_end - 1};
    const VAddr kmap_start{kernel_map_region_start};
    const VAddr kmap_last{kernel_map_region_end - 1};
    ASSERT(alias_last < heap_start || heap_last < alias_start);
    ASSERT(alias_last < stack_start || stack_last < alias_start);
    ASSERT(alias_last < kmap_start || kmap_last < alias_start);
    ASSERT(heap_last < stack_start || stack_last < heap_start);
    ASSERT(heap_last < kmap_start || kmap_last < heap_start);

    current_heap_addr = heap_region_start;
    heap_capacity = 0;
    physical_memory_usage = 0;
    memory_pool = pool;

    page_table_impl.Resize(address_space_width, PageBits);

    return InitializeMemoryLayout(start, end);
}

ResultCode PageTable::MapProcessCode(VAddr addr, std::size_t num_pages, MemoryState state,
                                     MemoryPermission perm) {
    std::lock_guard lock{page_table_lock};

    const u64 size{num_pages * PageSize};

    if (!CanContain(addr, size, state)) {
        return ResultInvalidCurrentMemory;
    }

    if (IsRegionMapped(addr, size)) {
        return ResultInvalidCurrentMemory;
    }

    PageLinkedList page_linked_list;
    CASCADE_CODE(
        system.Kernel().MemoryManager().Allocate(page_linked_list, num_pages, memory_pool));
    CASCADE_CODE(Operate(addr, num_pages, page_linked_list, OperationType::MapGroup));

    block_manager->Update(addr, num_pages, state, perm);

    return RESULT_SUCCESS;
}

ResultCode PageTable::MapProcessCodeMemory(VAddr dst_addr, VAddr src_addr, std::size_t size) {
    std::lock_guard lock{page_table_lock};

    const std::size_t num_pages{size / PageSize};

    MemoryState state{};
    MemoryPermission perm{};
    CASCADE_CODE(CheckMemoryState(&state, &perm, nullptr, src_addr, size, MemoryState::All,
                                  MemoryState::Normal, MemoryPermission::Mask,
                                  MemoryPermission::ReadAndWrite, MemoryAttribute::Mask,
                                  MemoryAttribute::None, MemoryAttribute::IpcAndDeviceMapped));

    if (IsRegionMapped(dst_addr, size)) {
        return ResultInvalidCurrentMemory;
    }

    PageLinkedList page_linked_list;
    AddRegionToPages(src_addr, num_pages, page_linked_list);

    {
        auto block_guard = detail::ScopeExit(
            [&] { Operate(src_addr, num_pages, perm, OperationType::ChangePermissions); });

        CASCADE_CODE(
            Operate(src_addr, num_pages, MemoryPermission::None, OperationType::ChangePermissions));
        CASCADE_CODE(MapPages(dst_addr, page_linked_list, MemoryPermission::None));

        block_guard.Cancel();
    }

    block_manager->Update(src_addr, num_pages, state, MemoryPermission::None,
                          MemoryAttribute::Locked);
    block_manager->Update(dst_addr, num_pages, MemoryState::AliasCode);

    return RESULT_SUCCESS;
}

ResultCode PageTable::UnmapProcessCodeMemory(VAddr dst_addr, VAddr src_addr, std::size_t size) {
    std::lock_guard lock{page_table_lock};

    if (!size) {
        return RESULT_SUCCESS;
    }

    const std::size_t num_pages{size / PageSize};

    CASCADE_CODE(CheckMemoryState(nullptr, nullptr, nullptr, src_addr, size, MemoryState::All,
                                  MemoryState::Normal, MemoryPermission::None,
                                  MemoryPermission::None, MemoryAttribute::Mask,
                                  MemoryAttribute::Locked, MemoryAttribute::IpcAndDeviceMapped));

    MemoryState state{};
    CASCADE_CODE(CheckMemoryState(
        &state, nullptr, nullptr, dst_addr, PageSize, MemoryState::FlagCanCodeAlias,
        MemoryState::FlagCanCodeAlias, MemoryPermission::None, MemoryPermission::None,
        MemoryAttribute::Mask, MemoryAttribute::None, MemoryAttribute::IpcAndDeviceMapped));
    CASCADE_CODE(CheckMemoryState(dst_addr, size, MemoryState::All, state, MemoryPermission::None,
                                  MemoryPermission::None, MemoryAttribute::Mask,
                                  MemoryAttribute::None));
    CASCADE_CODE(Operate(dst_addr, num_pages, MemoryPermission::None, OperationType::Unmap));

    block_manager->Update(dst_addr, num_pages, MemoryState::Free);
    block_manager->Update(src_addr, num_pages, MemoryState::Normal, MemoryPermission::ReadAndWrite);

    return RESULT_SUCCESS;
}

void PageTable::MapPhysicalMemory(PageLinkedList& page_linked_list, VAddr start, VAddr end) {
    auto node{page_linked_list.Nodes().begin()};
    PAddr map_addr{node->GetAddress()};
    std::size_t src_num_pages{node->GetNumPages()};

    block_manager->IterateForRange(start, end, [&](const MemoryInfo& info) {
        if (info.state != MemoryState::Free) {
            return;
        }

        std::size_t dst_num_pages{GetSizeInRange(info, start, end) / PageSize};
        VAddr dst_addr{GetAddressInRange(info, start)};

        while (dst_num_pages) {
            if (!src_num_pages) {
                node = std::next(node);
                map_addr = node->GetAddress();
                src_num_pages = node->GetNumPages();
            }

            const std::size_t num_pages{std::min(src_num_pages, dst_num_pages)};
            Operate(dst_addr, num_pages, MemoryPermission::ReadAndWrite, OperationType::Map,
                    map_addr);

            dst_addr += num_pages * PageSize;
            map_addr += num_pages * PageSize;
            src_num_pages -= num_pages;
            dst_num_pages -= num_pages;
        }
    });
}

ResultCode PageTable::MapPhysicalMemory(VAddr addr, std::size_t size) {
    std::lock_guard lock{page_table_lock};

    std::size_t mapped_size{};
    const VAddr end_addr{addr + size};

    block_manager->IterateForRange(addr, end_addr, [&](const MemoryInfo& info) {
        if (info.state != MemoryState::Free) {
            mapped_size += GetSizeInRange(info, addr, end_addr);
        }
    });

    if (mapped_size == size) {
        return RESULT_SUCCESS;
    }

    const std::size_t remaining_size{size - mapped_size};
    const std::size_t remaining_pages{remaining_size / PageSize};

    // Reserve the memory from the process resource limit.
    KScopedResourceReservation memory_reservation(
        system.Kernel().CurrentProcess()->GetResourceLimit(), LimitableResource::PhysicalMemory,
        remaining_size);
    if (!memory_reservation.Succeeded()) {
        LOG_ERROR(Kernel, "Could not reserve remaining {:X} bytes", remaining_size);
        return ResultResourceLimitedExceeded;
    }

    PageLinkedList page_linked_list;

    CASCADE_CODE(
        system.Kernel().MemoryManager().Allocate(page_linked_list, remaining_pages, memory_pool));

    // We succeeded, so commit the memory reservation.
    memory_reservation.Commit();

    MapPhysicalMemory(page_linked_list, addr, end_addr);

    physical_memory_usage += remaining_size;

    const std::size_t num_pages{size / PageSize};
    block_manager->Update(addr, num_pages, MemoryState::Free, MemoryPermission::None,
                          MemoryAttribute::None, MemoryState::Normal,
                          MemoryPermission::ReadAndWrite, MemoryAttribute::None);

    return RESULT_SUCCESS;
}

ResultCode PageTable::UnmapPhysicalMemory(VAddr addr, std::size_t size) {
    std::lock_guard lock{page_table_lock};

    const VAddr end_addr{addr + size};
    ResultCode result{RESULT_SUCCESS};
    std::size_t mapped_size{};

    // Verify that the region can be unmapped
    block_manager->IterateForRange(addr, end_addr, [&](const MemoryInfo& info) {
        if (info.state == MemoryState::Normal) {
            if (info.attribute != MemoryAttribute::None) {
                result = ResultInvalidCurrentMemory;
                return;
            }
            mapped_size += GetSizeInRange(info, addr, end_addr);
        } else if (info.state != MemoryState::Free) {
            result = ResultInvalidCurrentMemory;
        }
    });

    if (result.IsError()) {
        return result;
    }

    if (!mapped_size) {
        return RESULT_SUCCESS;
    }

    CASCADE_CODE(UnmapMemory(addr, size));

    auto process{system.Kernel().CurrentProcess()};
    process->GetResourceLimit()->Release(LimitableResource::PhysicalMemory, mapped_size);
    physical_memory_usage -= mapped_size;

    return RESULT_SUCCESS;
}

ResultCode PageTable::UnmapMemory(VAddr addr, std::size_t size) {
    std::lock_guard lock{page_table_lock};

    const VAddr end_addr{addr + size};
    ResultCode result{RESULT_SUCCESS};
    PageLinkedList page_linked_list;

    // Unmap each region within the range
    block_manager->IterateForRange(addr, end_addr, [&](const MemoryInfo& info) {
        if (info.state == MemoryState::Normal) {
            const std::size_t block_size{GetSizeInRange(info, addr, end_addr)};
            const std::size_t block_num_pages{block_size / PageSize};
            const VAddr block_addr{GetAddressInRange(info, addr)};

            AddRegionToPages(block_addr, block_size / PageSize, page_linked_list);

            if (result = Operate(block_addr, block_num_pages, MemoryPermission::None,
                                 OperationType::Unmap);
                result.IsError()) {
                return;
            }
        }
    });

    if (result.IsError()) {
        return result;
    }

    const std::size_t num_pages{size / PageSize};
    system.Kernel().MemoryManager().Free(page_linked_list, num_pages, memory_pool);

    block_manager->Update(addr, num_pages, MemoryState::Free);

    return RESULT_SUCCESS;
}

ResultCode PageTable::Map(VAddr dst_addr, VAddr src_addr, std::size_t size) {
    std::lock_guard lock{page_table_lock};

    MemoryState src_state{};
    CASCADE_CODE(CheckMemoryState(
        &src_state, nullptr, nullptr, src_addr, size, MemoryState::FlagCanAlias,
        MemoryState::FlagCanAlias, MemoryPermission::Mask, MemoryPermission::ReadAndWrite,
        MemoryAttribute::Mask, MemoryAttribute::None, MemoryAttribute::IpcAndDeviceMapped));

    if (IsRegionMapped(dst_addr, size)) {
        return ResultInvalidCurrentMemory;
    }

    PageLinkedList page_linked_list;
    const std::size_t num_pages{size / PageSize};

    AddRegionToPages(src_addr, num_pages, page_linked_list);

    {
        auto block_guard = detail::ScopeExit([&] {
            Operate(src_addr, num_pages, MemoryPermission::ReadAndWrite,
                    OperationType::ChangePermissions);
        });

        CASCADE_CODE(
            Operate(src_addr, num_pages, MemoryPermission::None, OperationType::ChangePermissions));
        CASCADE_CODE(MapPages(dst_addr, page_linked_list, MemoryPermission::ReadAndWrite));

        block_guard.Cancel();
    }

    block_manager->Update(src_addr, num_pages, src_state, MemoryPermission::None,
                          MemoryAttribute::Locked);
    block_manager->Update(dst_addr, num_pages, MemoryState::Stack, MemoryPermission::ReadAndWrite);

    return RESULT_SUCCESS;
}

ResultCode PageTable::Unmap(VAddr dst_addr, VAddr src_addr, std::size_t size) {
    std::lock_guard lock{page_table_lock};

    MemoryState src_state{};
    CASCADE_CODE(CheckMemoryState(
        &src_state, nullptr, nullptr, src_addr, size, MemoryState::FlagCanAlias,
        MemoryState::FlagCanAlias, MemoryPermission::Mask, MemoryPermission::None,
        MemoryAttribute::Mask, MemoryAttribute::Locked, MemoryAttribute::IpcAndDeviceMapped));

    MemoryPermission dst_perm{};
    CASCADE_CODE(CheckMemoryState(nullptr, &dst_perm, nullptr, dst_addr, size, MemoryState::All,
                                  MemoryState::Stack, MemoryPermission::None,
                                  MemoryPermission::None, MemoryAttribute::Mask,
                                  MemoryAttribute::None, MemoryAttribute::IpcAndDeviceMapped));

    PageLinkedList src_pages;
    PageLinkedList dst_pages;
    const std::size_t num_pages{size / PageSize};

    AddRegionToPages(src_addr, num_pages, src_pages);
    AddRegionToPages(dst_addr, num_pages, dst_pages);

    if (!dst_pages.IsEqual(src_pages)) {
        return ResultInvalidMemoryRange;
    }

    {
        auto block_guard = detail::ScopeExit([&] { MapPages(dst_addr, dst_pages, dst_perm); });

        CASCADE_CODE(Operate(dst_addr, num_pages, MemoryPermission::None, OperationType::Unmap));
        CASCADE_CODE(Operate(src_addr, num_pages, MemoryPermission::ReadAndWrite,
                             OperationType::ChangePermissions));

        block_guard.Cancel();
    }

    block_manager->Update(src_addr, num_pages, src_state, MemoryPermission::ReadAndWrite);
    block_manager->Update(dst_addr, num_pages, MemoryState::Free);

    return RESULT_SUCCESS;
}

ResultCode PageTable::MapPages(VAddr addr, const PageLinkedList& page_linked_list,
                               MemoryPermission perm) {
    VAddr cur_addr{addr};

    for (const auto& node : page_linked_list.Nodes()) {
        if (const auto result{
                Operate(cur_addr, node.GetNumPages(), perm, OperationType::Map, node.GetAddress())};
            result.IsError()) {
            const std::size_t num_pages{(addr - cur_addr) / PageSize};

            ASSERT(
                Operate(addr, num_pages, MemoryPermission::None, OperationType::Unmap).IsSuccess());

            return result;
        }

        cur_addr += node.GetNumPages() * PageSize;
    }

    return RESULT_SUCCESS;
}

ResultCode PageTable::MapPages(VAddr addr, PageLinkedList& page_linked_list, MemoryState state,
                               MemoryPermission perm) {
    std::lock_guard lock{page_table_lock};

    const std::size_t num_pages{page_linked_list.GetNumPages()};
    const std::size_t size{num_pages * PageSize};

    if (!CanContain(addr, size, state)) {
        return ResultInvalidCurrentMemory;
    }

    if (IsRegionMapped(addr, num_pages * PageSize)) {
        return ResultInvalidCurrentMemory;
    }

    CASCADE_CODE(MapPages(addr, page_linked_list, perm));

    block_manager->Update(addr, num_pages, state, perm);

    return RESULT_SUCCESS;
}

ResultCode PageTable::SetCodeMemoryPermission(VAddr addr, std::size_t size, MemoryPermission perm) {

    std::lock_guard lock{page_table_lock};

    MemoryState prev_state{};
    MemoryPermission prev_perm{};

    CASCADE_CODE(CheckMemoryState(
        &prev_state, &prev_perm, nullptr, addr, size, MemoryState::FlagCode, MemoryState::FlagCode,
        MemoryPermission::None, MemoryPermission::None, MemoryAttribute::Mask,
        MemoryAttribute::None, MemoryAttribute::IpcAndDeviceMapped));

    MemoryState state{prev_state};

    // Ensure state is mutable if permission allows write
    if ((perm & MemoryPermission::Write) != MemoryPermission::None) {
        if (prev_state == MemoryState::Code) {
            state = MemoryState::CodeData;
        } else if (prev_state == MemoryState::AliasCode) {
            state = MemoryState::AliasCodeData;
        } else {
            UNREACHABLE();
        }
    }

    // Return early if there is nothing to change
    if (state == prev_state && perm == prev_perm) {
        return RESULT_SUCCESS;
    }

    if ((prev_perm & MemoryPermission::Execute) != (perm & MemoryPermission::Execute)) {
        // Memory execution state is changing, invalidate CPU cache range
        system.InvalidateCpuInstructionCacheRange(addr, size);
    }

    const std::size_t num_pages{size / PageSize};
    const OperationType operation{(perm & MemoryPermission::Execute) != MemoryPermission::None
                                      ? OperationType::ChangePermissionsAndRefresh
                                      : OperationType::ChangePermissions};

    CASCADE_CODE(Operate(addr, num_pages, perm, operation));

    block_manager->Update(addr, num_pages, state, perm);

    return RESULT_SUCCESS;
}

MemoryInfo PageTable::QueryInfoImpl(VAddr addr) {
    std::lock_guard lock{page_table_lock};

    return block_manager->FindBlock(addr).GetMemoryInfo();
}

MemoryInfo PageTable::QueryInfo(VAddr addr) {
    if (!Contains(addr, 1)) {
        return {address_space_end,      0 - address_space_end, MemoryState::Inaccessible,
                MemoryPermission::None, MemoryAttribute::None, MemoryPermission::None};
    }

    return QueryInfoImpl(addr);
}

ResultCode PageTable::ReserveTransferMemory(VAddr addr, std::size_t size, MemoryPermission perm) {
    std::lock_guard lock{page_table_lock};

    MemoryState state{};
    MemoryAttribute attribute{};

    CASCADE_CODE(CheckMemoryState(&state, nullptr, &attribute, addr, size,
                                  MemoryState::FlagCanTransfer | MemoryState::FlagReferenceCounted,
                                  MemoryState::FlagCanTransfer | MemoryState::FlagReferenceCounted,
                                  MemoryPermission::Mask, MemoryPermission::ReadAndWrite,
                                  MemoryAttribute::Mask, MemoryAttribute::None,
                                  MemoryAttribute::IpcAndDeviceMapped));

    block_manager->Update(addr, size / PageSize, state, perm, attribute | MemoryAttribute::Locked);

    return RESULT_SUCCESS;
}

ResultCode PageTable::ResetTransferMemory(VAddr addr, std::size_t size) {
    std::lock_guard lock{page_table_lock};

    MemoryState state{};

    CASCADE_CODE(CheckMemoryState(&state, nullptr, nullptr, addr, size,
                                  MemoryState::FlagCanTransfer | MemoryState::FlagReferenceCounted,
                                  MemoryState::FlagCanTransfer | MemoryState::FlagReferenceCounted,
                                  MemoryPermission::None, MemoryPermission::None,
                                  MemoryAttribute::Mask, MemoryAttribute::Locked,
                                  MemoryAttribute::IpcAndDeviceMapped));

    block_manager->Update(addr, size / PageSize, state, MemoryPermission::ReadAndWrite);

    return RESULT_SUCCESS;
}

ResultCode PageTable::SetMemoryAttribute(VAddr addr, std::size_t size, MemoryAttribute mask,
                                         MemoryAttribute value) {
    std::lock_guard lock{page_table_lock};

    MemoryState state{};
    MemoryPermission perm{};
    MemoryAttribute attribute{};

    CASCADE_CODE(CheckMemoryState(&state, &perm, &attribute, addr, size,
                                  MemoryState::FlagCanChangeAttribute,
                                  MemoryState::FlagCanChangeAttribute, MemoryPermission::None,
                                  MemoryPermission::None, MemoryAttribute::LockedAndIpcLocked,
                                  MemoryAttribute::None, MemoryAttribute::DeviceSharedAndUncached));

    attribute = attribute & ~mask;
    attribute = attribute | (mask & value);

    block_manager->Update(addr, size / PageSize, state, perm, attribute);

    return RESULT_SUCCESS;
}

ResultCode PageTable::SetHeapCapacity(std::size_t new_heap_capacity) {
    std::lock_guard lock{page_table_lock};
    heap_capacity = new_heap_capacity;
    return RESULT_SUCCESS;
}

ResultVal<VAddr> PageTable::SetHeapSize(std::size_t size) {

    if (size > heap_region_end - heap_region_start) {
        return ResultOutOfMemory;
    }

    const u64 previous_heap_size{GetHeapSize()};

    UNIMPLEMENTED_IF_MSG(previous_heap_size > size, "Heap shrink is unimplemented");

    // Increase the heap size
    {
        std::lock_guard lock{page_table_lock};

        const u64 delta{size - previous_heap_size};

        // Reserve memory for the heap extension.
        KScopedResourceReservation memory_reservation(
            system.Kernel().CurrentProcess()->GetResourceLimit(), LimitableResource::PhysicalMemory,
            delta);

        if (!memory_reservation.Succeeded()) {
            LOG_ERROR(Kernel, "Could not reserve heap extension of size {:X} bytes", delta);
            return ResultResourceLimitedExceeded;
        }

        PageLinkedList page_linked_list;
        const std::size_t num_pages{delta / PageSize};

        CASCADE_CODE(
            system.Kernel().MemoryManager().Allocate(page_linked_list, num_pages, memory_pool));

        if (IsRegionMapped(current_heap_addr, delta)) {
            return ResultInvalidCurrentMemory;
        }

        CASCADE_CODE(
            Operate(current_heap_addr, num_pages, page_linked_list, OperationType::MapGroup));

        // Succeeded in allocation, commit the resource reservation
        memory_reservation.Commit();

        block_manager->Update(current_heap_addr, num_pages, MemoryState::Normal,
                              MemoryPermission::ReadAndWrite);

        current_heap_addr = heap_region_start + size;
    }

    return MakeResult<VAddr>(heap_region_start);
}

ResultVal<VAddr> PageTable::AllocateAndMapMemory(std::size_t needed_num_pages, std::size_t align,
                                                 bool is_map_only, VAddr region_start,
                                                 std::size_t region_num_pages, MemoryState state,
                                                 MemoryPermission perm, PAddr map_addr) {
    std::lock_guard lock{page_table_lock};

    if (!CanContain(region_start, region_num_pages * PageSize, state)) {
        return ResultInvalidCurrentMemory;
    }

    if (region_num_pages <= needed_num_pages) {
        return ResultOutOfMemory;
    }

    const VAddr addr{
        AllocateVirtualMemory(region_start, region_num_pages, needed_num_pages, align)};
    if (!addr) {
        return ResultOutOfMemory;
    }

    if (is_map_only) {
        CASCADE_CODE(Operate(addr, needed_num_pages, perm, OperationType::Map, map_addr));
    } else {
        PageLinkedList page_group;
        CASCADE_CODE(
            system.Kernel().MemoryManager().Allocate(page_group, needed_num_pages, memory_pool));
        CASCADE_CODE(Operate(addr, needed_num_pages, page_group, OperationType::MapGroup));
    }

    block_manager->Update(addr, needed_num_pages, state, perm);

    return MakeResult<VAddr>(addr);
}

ResultCode PageTable::LockForDeviceAddressSpace(VAddr addr, std::size_t size) {
    std::lock_guard lock{page_table_lock};

    MemoryPermission perm{};
    if (const ResultCode result{CheckMemoryState(
            nullptr, &perm, nullptr, addr, size, MemoryState::FlagCanChangeAttribute,
            MemoryState::FlagCanChangeAttribute, MemoryPermission::None, MemoryPermission::None,
            MemoryAttribute::LockedAndIpcLocked, MemoryAttribute::None,
            MemoryAttribute::DeviceSharedAndUncached)};
        result.IsError()) {
        return result;
    }

    block_manager->UpdateLock(
        addr, size / PageSize,
        [](MemoryBlockManager::iterator block, MemoryPermission perm) {
            block->ShareToDevice(perm);
        },
        perm);

    return RESULT_SUCCESS;
}

ResultCode PageTable::UnlockForDeviceAddressSpace(VAddr addr, std::size_t size) {
    std::lock_guard lock{page_table_lock};

    MemoryPermission perm{};
    if (const ResultCode result{CheckMemoryState(
            nullptr, &perm, nullptr, addr, size, MemoryState::FlagCanChangeAttribute,
            MemoryState::FlagCanChangeAttribute, MemoryPermission::None, MemoryPermission::None,
            MemoryAttribute::LockedAndIpcLocked, MemoryAttribute::None,
            MemoryAttribute::DeviceSharedAndUncached)};
        result.IsError()) {
        return result;
    }

    block_manager->UpdateLock(
        addr, size / PageSize,
        [](MemoryBlockManager::iterator block, MemoryPermission perm) {
            block->UnshareToDevice(perm);
        },
        perm);

    return RESULT_SUCCESS;
}

ResultCode PageTable::InitializeMemoryLayout(VAddr start, VAddr end) {
    block_manager = std::make_unique<MemoryBlockManager>(start, end);

    return RESULT_SUCCESS;
}

bool PageTable::IsRegionMapped(VAddr address, u64 size) {
    return CheckMemoryState(address, size, MemoryState::All, MemoryState::Free,
                            MemoryPermission::Mask, MemoryPermission::None, MemoryAttribute::Mask,
                            MemoryAttribute::None, MemoryAttribute::IpcAndDeviceMapped)
        .IsError();
}

bool PageTable::IsRegionContiguous(VAddr addr, u64 size) const {
    auto start_ptr = system.Memory().GetPointer(addr);
    for (u64 offset{}; offset < size; offset += PageSize) {
        if (start_ptr != system.Memory().GetPointer(addr + offset)) {
            return false;
        }
        start_ptr += PageSize;
    }
    return true;
}

void PageTable::AddRegionToPages(VAddr start, std::size_t num_pages,
                                 PageLinkedList& page_linked_list) {
    VAddr addr{start};
    while (addr < start + (num_pages * PageSize)) {
        const PAddr paddr{GetPhysicalAddr(addr)};
        if (!paddr) {
            UNREACHABLE();
        }
        page_linked_list.AddBlock(paddr, 1);
        addr += PageSize;
    }
}

VAddr PageTable::AllocateVirtualMemory(VAddr start, std::size_t region_num_pages,
                                       u64 needed_num_pages, std::size_t align) {
    if (is_aslr_enabled) {
        UNIMPLEMENTED();
    }
    return block_manager->FindFreeArea(start, region_num_pages, needed_num_pages, align, 0,
                                       IsKernel() ? 1 : 4);
}

ResultCode PageTable::Operate(VAddr addr, std::size_t num_pages, const PageLinkedList& page_group,
                              OperationType operation) {
    std::lock_guard lock{page_table_lock};

    ASSERT(Common::IsAligned(addr, PageSize));
    ASSERT(num_pages > 0);
    ASSERT(num_pages == page_group.GetNumPages());

    for (const auto& node : page_group.Nodes()) {
        const std::size_t size{node.GetNumPages() * PageSize};

        switch (operation) {
        case OperationType::MapGroup:
            system.Memory().MapMemoryRegion(page_table_impl, addr, size, node.GetAddress());
            break;
        default:
            UNREACHABLE();
        }

        addr += size;
    }

    return RESULT_SUCCESS;
}

ResultCode PageTable::Operate(VAddr addr, std::size_t num_pages, MemoryPermission perm,
                              OperationType operation, PAddr map_addr) {
    std::lock_guard lock{page_table_lock};

    ASSERT(num_pages > 0);
    ASSERT(Common::IsAligned(addr, PageSize));
    ASSERT(ContainsPages(addr, num_pages));

    switch (operation) {
    case OperationType::Unmap:
        system.Memory().UnmapRegion(page_table_impl, addr, num_pages * PageSize);
        break;
    case OperationType::Map: {
        ASSERT(map_addr);
        ASSERT(Common::IsAligned(map_addr, PageSize));
        system.Memory().MapMemoryRegion(page_table_impl, addr, num_pages * PageSize, map_addr);
        break;
    }
    case OperationType::ChangePermissions:
    case OperationType::ChangePermissionsAndRefresh:
        break;
    default:
        UNREACHABLE();
    }
    return RESULT_SUCCESS;
}

constexpr VAddr PageTable::GetRegionAddress(MemoryState state) const {
    switch (state) {
    case MemoryState::Free:
    case MemoryState::Kernel:
        return address_space_start;
    case MemoryState::Normal:
        return heap_region_start;
    case MemoryState::Ipc:
    case MemoryState::NonSecureIpc:
    case MemoryState::NonDeviceIpc:
        return alias_region_start;
    case MemoryState::Stack:
        return stack_region_start;
    case MemoryState::Io:
    case MemoryState::Static:
    case MemoryState::ThreadLocal:
        return kernel_map_region_start;
    case MemoryState::Shared:
    case MemoryState::AliasCode:
    case MemoryState::AliasCodeData:
    case MemoryState::Transferred:
    case MemoryState::SharedTransferred:
    case MemoryState::SharedCode:
    case MemoryState::GeneratedCode:
    case MemoryState::CodeOut:
        return alias_code_region_start;
    case MemoryState::Code:
    case MemoryState::CodeData:
        return code_region_start;
    default:
        UNREACHABLE();
        return {};
    }
}

constexpr std::size_t PageTable::GetRegionSize(MemoryState state) const {
    switch (state) {
    case MemoryState::Free:
    case MemoryState::Kernel:
        return address_space_end - address_space_start;
    case MemoryState::Normal:
        return heap_region_end - heap_region_start;
    case MemoryState::Ipc:
    case MemoryState::NonSecureIpc:
    case MemoryState::NonDeviceIpc:
        return alias_region_end - alias_region_start;
    case MemoryState::Stack:
        return stack_region_end - stack_region_start;
    case MemoryState::Io:
    case MemoryState::Static:
    case MemoryState::ThreadLocal:
        return kernel_map_region_end - kernel_map_region_start;
    case MemoryState::Shared:
    case MemoryState::AliasCode:
    case MemoryState::AliasCodeData:
    case MemoryState::Transferred:
    case MemoryState::SharedTransferred:
    case MemoryState::SharedCode:
    case MemoryState::GeneratedCode:
    case MemoryState::CodeOut:
        return alias_code_region_end - alias_code_region_start;
    case MemoryState::Code:
    case MemoryState::CodeData:
        return code_region_end - code_region_start;
    default:
        UNREACHABLE();
        return {};
    }
}

constexpr bool PageTable::CanContain(VAddr addr, std::size_t size, MemoryState state) const {
    const VAddr end{addr + size};
    const VAddr last{end - 1};
    const VAddr region_start{GetRegionAddress(state)};
    const std::size_t region_size{GetRegionSize(state)};
    const bool is_in_region{region_start <= addr && addr < end &&
                            last <= region_start + region_size - 1};
    const bool is_in_heap{!(end <= heap_region_start || heap_region_end <= addr)};
    const bool is_in_alias{!(end <= alias_region_start || alias_region_end <= addr)};

    switch (state) {
    case MemoryState::Free:
    case MemoryState::Kernel:
        return is_in_region;
    case MemoryState::Io:
    case MemoryState::Static:
    case MemoryState::Code:
    case MemoryState::CodeData:
    case MemoryState::Shared:
    case MemoryState::AliasCode:
    case MemoryState::AliasCodeData:
    case MemoryState::Stack:
    case MemoryState::ThreadLocal:
    case MemoryState::Transferred:
    case MemoryState::SharedTransferred:
    case MemoryState::SharedCode:
    case MemoryState::GeneratedCode:
    case MemoryState::CodeOut:
        return is_in_region && !is_in_heap && !is_in_alias;
    case MemoryState::Normal:
        ASSERT(is_in_heap);
        return is_in_region && !is_in_alias;
    case MemoryState::Ipc:
    case MemoryState::NonSecureIpc:
    case MemoryState::NonDeviceIpc:
        ASSERT(is_in_alias);
        return is_in_region && !is_in_heap;
    default:
        return false;
    }
}

constexpr ResultCode PageTable::CheckMemoryState(const MemoryInfo& info, MemoryState state_mask,
                                                 MemoryState state, MemoryPermission perm_mask,
                                                 MemoryPermission perm, MemoryAttribute attr_mask,
                                                 MemoryAttribute attr) const {
    // Validate the states match expectation
    if ((info.state & state_mask) != state) {
        return ResultInvalidCurrentMemory;
    }
    if ((info.perm & perm_mask) != perm) {
        return ResultInvalidCurrentMemory;
    }
    if ((info.attribute & attr_mask) != attr) {
        return ResultInvalidCurrentMemory;
    }

    return RESULT_SUCCESS;
}

ResultCode PageTable::CheckMemoryState(MemoryState* out_state, MemoryPermission* out_perm,
                                       MemoryAttribute* out_attr, VAddr addr, std::size_t size,
                                       MemoryState state_mask, MemoryState state,
                                       MemoryPermission perm_mask, MemoryPermission perm,
                                       MemoryAttribute attr_mask, MemoryAttribute attr,
                                       MemoryAttribute ignore_attr) {
    std::lock_guard lock{page_table_lock};

    // Get information about the first block
    const VAddr last_addr{addr + size - 1};
    MemoryBlockManager::const_iterator it{block_manager->FindIterator(addr)};
    MemoryInfo info{it->GetMemoryInfo()};

    // Validate all blocks in the range have correct state
    const MemoryState first_state{info.state};
    const MemoryPermission first_perm{info.perm};
    const MemoryAttribute first_attr{info.attribute};

    while (true) {
        // Validate the current block
        if (!(info.state == first_state)) {
            return ResultInvalidCurrentMemory;
        }
        if (!(info.perm == first_perm)) {
            return ResultInvalidCurrentMemory;
        }
        if (!((info.attribute | static_cast<MemoryAttribute>(ignore_attr)) ==
              (first_attr | static_cast<MemoryAttribute>(ignore_attr)))) {
            return ResultInvalidCurrentMemory;
        }

        // Validate against the provided masks
        CASCADE_CODE(CheckMemoryState(info, state_mask, state, perm_mask, perm, attr_mask, attr));

        // Break once we're done
        if (last_addr <= info.GetLastAddress()) {
            break;
        }

        // Advance our iterator
        it++;
        ASSERT(it != block_manager->cend());
        info = it->GetMemoryInfo();
    }

    // Write output state
    if (out_state) {
        *out_state = first_state;
    }
    if (out_perm) {
        *out_perm = first_perm;
    }
    if (out_attr) {
        *out_attr = first_attr & static_cast<MemoryAttribute>(~ignore_attr);
    }

    return RESULT_SUCCESS;
}

} // namespace Kernel::Memory
