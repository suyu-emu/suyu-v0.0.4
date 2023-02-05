// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/core.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/svc.h"

namespace Kernel::Svc {

/// Set the process heap to a given Size. It can both extend and shrink the heap.
Result SetHeapSize(Core::System& system, VAddr* out_address, u64 size) {
    LOG_TRACE(Kernel_SVC, "called, heap_size=0x{:X}", size);

    // Validate size.
    R_UNLESS(Common::IsAligned(size, HeapSizeAlignment), ResultInvalidSize);
    R_UNLESS(size < MainMemorySizeMax, ResultInvalidSize);

    // Set the heap size.
    R_TRY(system.Kernel().CurrentProcess()->PageTable().SetHeapSize(out_address, size));

    return ResultSuccess;
}

Result SetHeapSize32(Core::System& system, u32* heap_addr, u32 heap_size) {
    VAddr temp_heap_addr{};
    const Result result{SetHeapSize(system, &temp_heap_addr, heap_size)};
    *heap_addr = static_cast<u32>(temp_heap_addr);
    return result;
}

/// Maps memory at a desired address
Result MapPhysicalMemory(Core::System& system, VAddr addr, u64 size) {
    LOG_DEBUG(Kernel_SVC, "called, addr=0x{:016X}, size=0x{:X}", addr, size);

    if (!Common::Is4KBAligned(addr)) {
        LOG_ERROR(Kernel_SVC, "Address is not aligned to 4KB, 0x{:016X}", addr);
        return ResultInvalidAddress;
    }

    if (!Common::Is4KBAligned(size)) {
        LOG_ERROR(Kernel_SVC, "Size is not aligned to 4KB, 0x{:X}", size);
        return ResultInvalidSize;
    }

    if (size == 0) {
        LOG_ERROR(Kernel_SVC, "Size is zero");
        return ResultInvalidSize;
    }

    if (!(addr < addr + size)) {
        LOG_ERROR(Kernel_SVC, "Size causes 64-bit overflow of address");
        return ResultInvalidMemoryRegion;
    }

    KProcess* const current_process{system.Kernel().CurrentProcess()};
    auto& page_table{current_process->PageTable()};

    if (current_process->GetSystemResourceSize() == 0) {
        LOG_ERROR(Kernel_SVC, "System Resource Size is zero");
        return ResultInvalidState;
    }

    if (!page_table.IsInsideAddressSpace(addr, size)) {
        LOG_ERROR(Kernel_SVC,
                  "Address is not within the address space, addr=0x{:016X}, size=0x{:016X}", addr,
                  size);
        return ResultInvalidMemoryRegion;
    }

    if (page_table.IsOutsideAliasRegion(addr, size)) {
        LOG_ERROR(Kernel_SVC,
                  "Address is not within the alias region, addr=0x{:016X}, size=0x{:016X}", addr,
                  size);
        return ResultInvalidMemoryRegion;
    }

    return page_table.MapPhysicalMemory(addr, size);
}

Result MapPhysicalMemory32(Core::System& system, u32 addr, u32 size) {
    return MapPhysicalMemory(system, addr, size);
}

/// Unmaps memory previously mapped via MapPhysicalMemory
Result UnmapPhysicalMemory(Core::System& system, VAddr addr, u64 size) {
    LOG_DEBUG(Kernel_SVC, "called, addr=0x{:016X}, size=0x{:X}", addr, size);

    if (!Common::Is4KBAligned(addr)) {
        LOG_ERROR(Kernel_SVC, "Address is not aligned to 4KB, 0x{:016X}", addr);
        return ResultInvalidAddress;
    }

    if (!Common::Is4KBAligned(size)) {
        LOG_ERROR(Kernel_SVC, "Size is not aligned to 4KB, 0x{:X}", size);
        return ResultInvalidSize;
    }

    if (size == 0) {
        LOG_ERROR(Kernel_SVC, "Size is zero");
        return ResultInvalidSize;
    }

    if (!(addr < addr + size)) {
        LOG_ERROR(Kernel_SVC, "Size causes 64-bit overflow of address");
        return ResultInvalidMemoryRegion;
    }

    KProcess* const current_process{system.Kernel().CurrentProcess()};
    auto& page_table{current_process->PageTable()};

    if (current_process->GetSystemResourceSize() == 0) {
        LOG_ERROR(Kernel_SVC, "System Resource Size is zero");
        return ResultInvalidState;
    }

    if (!page_table.IsInsideAddressSpace(addr, size)) {
        LOG_ERROR(Kernel_SVC,
                  "Address is not within the address space, addr=0x{:016X}, size=0x{:016X}", addr,
                  size);
        return ResultInvalidMemoryRegion;
    }

    if (page_table.IsOutsideAliasRegion(addr, size)) {
        LOG_ERROR(Kernel_SVC,
                  "Address is not within the alias region, addr=0x{:016X}, size=0x{:016X}", addr,
                  size);
        return ResultInvalidMemoryRegion;
    }

    return page_table.UnmapPhysicalMemory(addr, size);
}

Result UnmapPhysicalMemory32(Core::System& system, u32 addr, u32 size) {
    return UnmapPhysicalMemory(system, addr, size);
}

} // namespace Kernel::Svc
